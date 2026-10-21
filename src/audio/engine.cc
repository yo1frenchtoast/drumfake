#include "audio/engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/sample_pool.h"
#include "core/scales.h"

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace drumfake {

const float kRepeatPeriods[kNumRepeatRates] = {
  4.0f, 2.0f, 1.0f, 0.5f, 8.0f / 3.0f, 4.0f / 3.0f, 2.0f / 3.0f, 1.0f / 3.0f
};

const char* const kRepeatNames[kNumRepeatRates] = {
  "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/32T"
};

namespace {

// Loop lengths (in steps) selected by the Y axis in looper mode.
const double kLoopLengths[] = { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 };
const int kNumLoopLengths = sizeof(kLoopLengths) / sizeof(kLoopLengths[0]);

inline uint32_t Hash(uint64_t x) {
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return static_cast<uint32_t>(x);
}

inline int64_t PositiveModulo(int64_t a, int64_t b) {
  int64_t m = a % b;
  return m < 0 ? m + b : m;
}

}  // namespace

Engine::Engine()
    : project_(new Project()),
      master_fx_(new MasterFx()),
      clouds_fx_(new CloudsFx()) {
}

Engine::~Engine() { }

void Engine::Init() {
  project_->Init();
  for (int i = 0; i < kChordVoices; ++i) {
    chord_voices_[i].Init(static_cast<uint32_t>(101 + i));
    chord_owner_[i] = 0;
    chord_started_[i] = 0;
  }
  for (int i = 0; i < kNumTracks; ++i) {
    voices_[i].Init(static_cast<uint32_t>(1 + i));
    track_step[i] = -1;
    trigger_count[i] = 0;
    track_peak[i] = 0.0f;
    track_gate[i] = false;
    last_trigger_sample[i] = -1;
    last_trigger_velocity[i] = 0.0f;
    active_lock_mask[i] = 0;
    skip_step_[i] = -1;
    for (int p = 0; p < kNumLockable; ++p) {
      active_locks_[i][p] = NAN;
      lock_layer_[i][p] = NAN;
      slide_from_[i][p] = NAN;
    }
    slide_total_[i] = 0;
    slide_left_[i] = 0;
  }
  for (int i = 0; i < kNumTracks; ++i) {
    for (int p = 0; p < kNumLockable; ++p) {
      effective_param[i][p] = project_->sounds[i].params[p];
    }
  }
  master_fx_->Init();
  clouds_fx_->Init();
  grids_.Init(0x5eed);
  master_peak = 0.0f;
  pattern_step = -1;
  for (int i = 0; i < kScopeSize; ++i) {
    scope[i] = 0.0f;
  }
  scope_write = 0;
  cpu_load = 0.0f;
  midi_activity = 0;
  midi_input_ports = 0;
  resample_progress = 0.0f;
  clock_ticks_ = -1;
  last_tick_time_ = -1.0;
  tick_interval_ = 0.0;

  playing_ = false;
  panel_ = false;
  snapshot_armed_ = false;
  song_mode_ = false;
  song_position_ = 0;
  current_pattern_ = 0;
  queued_pattern_ = -1;
  position_ = 0.0;
  beat_position_ = 0.0;
  free_position_ = 0.0;
  pattern_start_ = 0;
  poly_origin_ = 0;
  last_grids_step_ = -1;
  sample_counter_ = 0;
  repeat_mask_ = 0;
  repeat_rate_ = 2;
  looping_ = false;
  loop_start_ = 0.0;
  loop_length_ = 4.0;
  click_phase_ = 0.0f;
  click_envelope_ = 0.0f;
  click_frequency_ = 1000.0f;
  block_read_ = kBlockSize;
}

void Engine::Play() {
  if (song_mode_ && project_->song_length > 0) {
    // A song always starts from the top of the chain.
    song_position_ = 0;
    ChangePattern(project_->song[0]);
    queued_pattern_ = -1;
  }
  if (queued_pattern_ >= 0) {
    ChangePattern(queued_pattern_);
    queued_pattern_ = -1;
  }
  playing_ = true;
  pattern_started_ = true;
  clock_ticks_ = -1;  // With an external clock, wait for its first tick.
  position_ = 0.0;
  beat_position_ = 0.0;
  free_position_ = 0.0;
  pattern_start_ = 0;
  poly_origin_ = 0;
  last_grids_step_ = -1;
  looping_ = false;
  for (int i = 0; i < kNumTracks; ++i) {
    skip_step_[i] = -1;
  }
}

void Engine::Stop() {
  playing_ = false;
  looping_ = false;
  pattern_step = -1;
  for (int i = 0; i < kNumTracks; ++i) {
    track_step[i] = -1;
  }
}

void Engine::SetSongMode(bool enabled) {
  song_mode_ = enabled && project_->song_length > 0;
  queued_pattern_ = -1;
  if (!song_mode_) {
    return;
  }
  if (playing_) {
    // Start the chain at the end of the pattern playing now.
    song_position_ = -1;
  } else {
    song_position_ = 0;
    ChangePattern(project_->song[0]);
  }
}

float Engine::tempo() const {
  if (project_->midi_clock_sync && tick_interval_ > 0.0) {
    return std::clamp(static_cast<float>(60.0 / (tick_interval_ * 24.0)), 20.0f, 400.0f);
  }
  return project_->bpm;
}

void Engine::HandleMidi(const MidiMessage& message, double time) {
  midi_activity.fetch_add(1, std::memory_order_relaxed);
  if (midi_monitor && message.type() != MIDI_CLOCK) {
    fprintf(stderr, "MIDI in: %02x %02x %02x\n", message.status,
            message.size > 1 ? message.data[0] : 0, message.size > 2 ? message.data[1] : 0);
  }
  const bool sync = project_->midi_clock_sync;
  switch (message.type()) {
    case MIDI_NOTE_ON: {
      if (message.data[1] == 0) {
        break;  // Note off.
      }
      const int channel = message.channel() + 1;
      const int note = message.data[0];
      const float velocity = message.data[1] / 127.0f;
      bool played = false;
      for (int t = 0; t < kNumTracks; ++t) {
        if (project_->midi_channels[t] == channel) {
          // The track's own channel: play the note at its pitch.
          int track_note = static_cast<int>(
              std::lround(PitchToNote(project_->sounds[t].params[P_PITCH])));
          LiveTrigger(t, velocity, note - track_note);
          played = true;
        }
      }
      if (!played) {
        // Other channels: the major scale from the trigger note plays A-H.
        int track = MajorScaleDegree(project_->midi_trigger_note, note);
        if (track >= 0) {
          LiveTrigger(track, velocity, 0);
        }
      }
      break;
    }
    case MIDI_CLOCK: {
      double interval = time - last_tick_time_;
      if (last_tick_time_ >= 0.0 && interval > 0.0 && interval < 0.25) {
        tick_interval_ = tick_interval_ > 0.0
            ? tick_interval_ + (interval - tick_interval_) * 0.08
            : interval;
      }
      last_tick_time_ = time;
      if (sync && playing_) {
        ++clock_ticks_;
      }
      break;
    }
    case MIDI_START:
      if (sync) {
        Play();
      }
      break;
    case MIDI_CONTINUE:
      if (sync && !playing_) {
        playing_ = true;
        looping_ = false;
        clock_ticks_ = static_cast<int64_t>(std::llround(beat_position_ * 6.0)) - 1;
      }
      break;
    case MIDI_STOP:
      if (sync) {
        Stop();
      }
      break;
    case MIDI_SONG_POSITION:
      if (sync && !playing_) {
        // In MIDI beats (16th notes).
        int64_t sixteenths = message.data[0] | (message.data[1] << 7);
        int length = project_->patterns[current_pattern_].length;
        position_ = sixteenths / StepLength();
        int64_t steps = static_cast<int64_t>(std::floor(position_));
        pattern_start_ = steps - steps % length;
        poly_origin_ = 0;
        beat_position_ = static_cast<double>(sixteenths);
        clock_ticks_ = sixteenths * 6 - 1;
      }
      break;
    default:
      break;
  }
}

void Engine::SelectPattern(int pattern) {
  song_mode_ = false;
  if (!playing_ || !project_->switch_at_end) {
    // Stopped, or At End off: switch now (playing, the step count goes on).
    ChangePattern(pattern);
    queued_pattern_ = -1;
  } else {
    queued_pattern_ = pattern == current_pattern_ ? -1 : pattern;
  }
}

void Engine::ChangePattern(int pattern) {
  if (pattern == current_pattern_) {
    return;
  }
  if (!panel_) {
    StoreKit();
    std::copy(project_->patterns[pattern].kit, project_->patterns[pattern].kit + kNumTracks,
              project_->sounds);
  }
  current_pattern_ = pattern;
}

void Engine::StoreKit() {
  std::copy(project_->sounds, project_->sounds + kNumTracks,
            project_->patterns[current_pattern_].kit);
}

void Engine::ReloadKit() {
  if (!panel_) {
    const TrackSound* kit = project_->patterns[current_pattern_].kit;
    std::copy(kit, kit + kNumTracks, project_->sounds);
  }
}

void Engine::ArmSnapshot() {
  if (snapshot_armed_) {
    return;
  }
  Snapshot& s = snapshot_;
  std::copy(project_->sounds, project_->sounds + kNumTracks, s.sounds);
  std::copy(project_->master, project_->master + M_LAST, s.master);
  std::copy(project_->assign, project_->assign + A_LAST, s.assign);
  std::copy(project_->clouds, project_->clouds + C_LAST, s.clouds);
  s.bpm = project_->bpm;
  s.volume = project_->volume;
  std::copy(muted, muted + kNumTracks, s.muted);
  std::copy(soloed, soloed + kNumTracks, s.soloed);
  s.grids_enabled = grids_enabled;
  s.grids_x = grids_x;
  s.grids_y = grids_y;
  s.clouds_freeze = clouds_freeze;
  s.key_root = project_->key_root;
  s.key_scale = project_->key_scale;
  s.key_octave = project_->key_octave;
  s.chord_type = project_->chord_type;
  s.panel = panel_;
  SetPanel(true);  // Stores the kit, then edits stay out of the patterns.
  snapshot_armed_ = true;
}

void Engine::RestoreSnapshot() {
  if (!snapshot_armed_) {
    return;
  }
  const Snapshot& s = snapshot_;
  snapshot_armed_ = false;
  std::copy(s.master, s.master + M_LAST, project_->master);
  std::copy(s.assign, s.assign + A_LAST, project_->assign);
  std::copy(s.clouds, s.clouds + C_LAST, project_->clouds);
  project_->bpm = s.bpm;
  project_->volume = s.volume;
  std::copy(s.muted, s.muted + kNumTracks, muted);
  std::copy(s.soloed, s.soloed + kNumTracks, soloed);
  grids_enabled = s.grids_enabled;
  grids_x = s.grids_x;
  grids_y = s.grids_y;
  clouds_freeze = s.clouds_freeze;
  project_->key_root = s.key_root;
  project_->key_scale = s.key_scale;
  project_->key_octave = s.key_octave;
  project_->chord_type = s.chord_type;
  if (s.panel) {
    // Panel mode was on: the sound playing before arming comes back.
    std::copy(s.sounds, s.sounds + kNumTracks, project_->sounds);
  } else {
    // Back to the kit of the current pattern (the one of the snapshot, unless the pattern
    // changed meanwhile).
    SetPanel(false);
  }
}

void Engine::SetPanel(bool panel) {
  if (panel == panel_) {
    return;
  }
  if (panel) {
    StoreKit();  // Keep the edits made so far in the pattern.
  }
  panel_ = panel;
  if (!panel) {
    ReloadKit();  // Panel edits that were not stored are dropped.
  }
}

void Engine::SetRepeat(uint8_t mask, int rate) {
  repeat_mask_ = mask;
  repeat_rate_ = std::max(0, std::min(kNumRepeatRates - 1, rate));
}

double Engine::StepLength() const {
  return kDivisionSteps[project_->patterns[current_pattern_].division];
}

size_t Engine::SamplesPerSixteenth() const {
  return static_cast<size_t>(kSampleRate * 15.0f / tempo());
}

size_t Engine::SamplesPerStep() const {
  return static_cast<size_t>(SamplesPerSixteenth() * StepLength());
}

bool Engine::RollProbability(int64_t step, int track, float probability) const {
  if (probability >= 1.0f) {
    return true;
  }
  uint32_t h = Hash(static_cast<uint64_t>(step) * 16 + track);
  return static_cast<float>(h) / 4294967296.0f < probability;
}

bool Engine::VaryStep(const Pattern& pattern, int track, int s, const Step& stored,
                      Step* out) const {
  const float amount = pattern.vary;
  // One draw per step, track and aspect: the variation repeats on every loop.
  auto draw = [&](int aspect) {
    uint64_t key = (static_cast<uint64_t>(pattern.vary_seed) << 32) ^
        (static_cast<uint64_t>(track * kMaxSteps + s) * 8 + aspect);
    return static_cast<float>(Hash(key)) / 4294967296.0f;
  };
  const TrackSequence& sequence = pattern.tracks[track];
  if (!stored.on) {
    // Ghost note: a soft echo of the previous hit of the track (none on an empty track).
    if (draw(0) >= amount * 0.15f) {
      return false;
    }
    const int length = pattern.track_length(track);
    for (int back = 1; back < length; ++back) {
      const Step& previous = sequence.steps[(s - back + length) % length];
      if (previous.on) {
        *out = previous;
        out->velocity = previous.velocity * (0.25f + 0.2f * draw(1));
        out->probability = 1.0f;
        out->ratchets = 1;
        out->micro = 0.0f;
        out->slide = false;
        return true;
      }
    }
    return false;
  }
  if (draw(0) < amount * 0.3f) {
    return false;  // Dropped hit.
  }
  *out = stored;
  out->velocity = stored.velocity * (1.0f - 0.6f * amount * draw(1));
  out->micro = std::clamp(stored.micro + amount * 0.08f * (2.0f * draw(2) - 1.0f),
                          -kMaxMicroTiming, kMaxMicroTiming);
  if (stored.ratchets == 1 && draw(3) < amount * 0.12f) {
    out->ratchets = 2;
  }
  // Shorter decay, on the step's own lock when it has one.
  float decay = stored.has_lock(P_DECAY) ? stored.locks[P_DECAY]
                                         : project_->sounds[track].params[P_DECAY];
  out->locks[P_DECAY] = decay * (1.0f - 0.5f * amount * draw(4));
  return true;
}

void Engine::SwapSample(int slot, std::shared_ptr<const SampleData>* sample) {
  if (slot >= 0 && slot < kNumSampleSlots) {
    samples_[slot].swap(*sample);
  }
}

void Engine::SwapSamples(std::shared_ptr<const SampleData>* samples) {
  for (int i = 0; i < kNumSampleSlots; ++i) {
    samples_[i].swap(samples[i]);
  }
}

const SampleData* Engine::sample(int slot) const {
  return slot >= 0 && slot < kNumSampleSlots ? samples_[slot].get() : nullptr;
}

const SampleData* Engine::VoiceSample(const float* params) const {
  if (!IsSampleEngine(Discrete(params[P_ENGINE], kNumEngines))) {
    return nullptr;
  }
  return samples_[SampleSlot(params[P_AUX])].get();
}

void Engine::ArmResample(std::vector<int16_t>* buffer) {
  resample_buffer_.swap(*buffer);
  resample_state_ = resample_buffer_.empty() ? RESAMPLE_IDLE : RESAMPLE_ARMED;
  resample_length_ = 0;
  resample_progress.store(0.0f, std::memory_order_relaxed);
}

void Engine::CancelResample(std::vector<int16_t>* buffer) {
  buffer->swap(resample_buffer_);
  resample_state_ = RESAMPLE_IDLE;
}

bool Engine::TakeResample(std::vector<int16_t>* frames) {
  if (resample_state_ != RESAMPLE_DONE) {
    return false;
  }
  frames->swap(resample_buffer_);
  frames->resize(resample_length_);  // Shrinks only: on the UI thread, after the swap.
  resample_state_ = RESAMPLE_IDLE;
  return true;
}

void Engine::RecordResample(const float* out_l, const float* out_r) {
  const bool started = pattern_started_;
  pattern_started_ = false;
  if (resample_state_ == RESAMPLE_ARMED && started && playing_) {
    const Pattern& pattern = project_->patterns[current_pattern_];
    resample_state_ = RESAMPLE_RECORDING;
    resample_length_ = 0;
    resample_target_ = std::min(resample_buffer_.size(),
                                static_cast<size_t>(pattern.length) * SamplesPerStep());
  }
  if (resample_state_ != RESAMPLE_RECORDING) {
    return;
  }
  for (size_t i = 0; i < kBlockSize && resample_length_ < resample_target_; ++i) {
    float mono = std::clamp(0.5f * (out_l[i] + out_r[i]), -1.0f, 1.0f);
    resample_buffer_[resample_length_++] = static_cast<int16_t>(std::lround(mono * 32767.0f));
  }
  resample_progress.store(static_cast<float>(resample_length_) / std::max<size_t>(1, resample_target_),
                          std::memory_order_relaxed);
  // Done at the end of the loop, or when the transport stops.
  if (resample_length_ >= resample_target_ || !playing_) {
    resample_state_ = RESAMPLE_DONE;
  }
}

void Engine::DiceVary() {
  Pattern& pattern = project_->patterns[current_pattern_];
  pattern.vary_seed = Hash((static_cast<uint64_t>(pattern.vary_seed) << 32) ^
                           static_cast<uint64_t>(sample_counter_)) | 1;
}

int Engine::TrackStep(const Pattern& pattern, int track, int64_t n) const {
  // In poly mode, tracks only realign on Play and on pattern changes; they
  // keep cycling on their own length when the pattern loops.
  int64_t position = pattern.poly
      ? n - poly_origin_
      : PositiveModulo(n - pattern_start_, pattern.length);
  return static_cast<int>(PositiveModulo(position, pattern.track_length(track)));
}

bool Engine::TrackAudible(int track) const {
  if (muted[track]) {
    return false;
  }
  for (int t = 0; t < kNumTracks; ++t) {
    if (soloed[t]) {
      return soloed[track];
    }
  }
  return true;
}

void Engine::LiveTrigger(int track, float velocity, int note_offset) {
  FireTrack(track, velocity, note_offset, nullptr, SamplesPerStep() * 16);
  RecordHit(track, velocity, note_offset, 1);
}

void Engine::LiveChord(int track, float velocity, int note_offset, const int8_t* extras,
                       int count) {
  const size_t gate = SamplesPerStep() * 16;
  FireTrack(track, velocity, note_offset, nullptr, gate);
  FireChord(track, velocity, note_offset, extras, count, gate, 0);
  RecordHit(track, velocity, note_offset, 1, extras, count);
}

void Engine::LiveTriggerLocked(int track, float velocity, const float* locks) {
  FireTrack(track, velocity, 0, locks, SamplesPerStep() * 16);
  RecordHit(track, velocity, 0, 1, nullptr, 0, locks);
}

void Engine::RecordHit(int track, float velocity, int note_offset, int ratchets,
                       const int8_t* extras, int count, const float* locks) {
  if (!recording || !playing_ || looping_) {
    return;
  }
  Pattern& pattern = project_->patterns[current_pattern_];
  int64_t n = static_cast<int64_t>(std::llround(position_));
  Step& step = pattern.tracks[track].steps[TrackStep(pattern, track, n)];
  step.on = true;
  step.velocity = velocity;
  step.note = note_offset;
  step.ratchets = ratchets;
  step.probability = 1.0f;
  step.chord_size = static_cast<uint8_t>(std::min(count, kMaxChordNotes - 1));
  for (int i = 0; i < step.chord_size; ++i) {
    step.chord[i] = extras[i];
  }
  if (locks) {
    for (int p = 0; p < kNumLockable; ++p) {
      if (!std::isnan(locks[p])) {
        step.locks[p] = locks[p];
      }
    }
  }
  if (static_cast<double>(n) > position_) {
    // Rounded to an upcoming step: don't play it twice.
    skip_step_[track] = n;
  }
}

void Engine::ApplyKit(const TrackSound* kit, bool all_patterns) {
  std::copy(kit, kit + kNumTracks, project_->sounds);
  if (all_patterns) {
    for (Pattern& pattern : project_->patterns) {
      std::copy(kit, kit + kNumTracks, pattern.kit);
    }
  } else if (!panel_) {
    StoreKit();
  }
}

void Engine::BakeGrids() {
  GridsGenerator generator;
  generator.Init(0x5eed);
  const float* a = project_->assign;
  uint8_t density[kGridsParts];
  for (int i = 0; i < kGridsParts; ++i) {
    density[i] = static_cast<uint8_t>(a[A_GRIDS_DENSITY_1 + i] * 255.0f);
  }
  uint8_t randomness = static_cast<uint8_t>(a[A_GRIDS_CHAOS] * 255.0f);
  uint8_t x = static_cast<uint8_t>(grids_x * 255.0f);
  uint8_t y = static_cast<uint8_t>(grids_y * 255.0f);
  Pattern& pattern = project_->patterns[current_pattern_];
  uint8_t bits[16];
  uint8_t accents[16];
  for (int s = 0; s < 16; ++s) {
    bits[s] = generator.EvaluateSixteenth(s, x, y, density, randomness, &accents[s]);
  }
  for (int part = 0; part < kGridsParts; ++part) {
    for (int s = 0; s < pattern.length; ++s) {
      Step& step = pattern.tracks[part].steps[s];
      step.on = bits[s % 16] & (1 << part);
      step.velocity = (accents[s % 16] & (1 << part)) ? 1.0f : 0.7f;
      step.ratchets = 1;
      step.probability = 1.0f;
    }
  }
}

void Engine::BakeMarbles(uint32_t seed) {
  Pattern& pattern = project_->patterns[current_pattern_];
  bool t1[kMaxSteps], t3[kMaxSteps];
  MarblesGenerate(project_->marbles, seed, pattern.length, t1, t3);
  const bool* hits[2] = { t1, t3 };
  for (int output = 0; output < 2; ++output) {
    TrackSequence& track = pattern.tracks[project_->marbles_tracks[output]];
    for (int s = 0; s < pattern.length; ++s) {
      Step& step = track.steps[s];
      step.on = hits[output][s];
      step.velocity = 0.8f;
      step.ratchets = 1;
      step.probability = 1.0f;
    }
  }
}

void Engine::FireTrack(int track, float velocity, int note, const float* locks,
                       size_t max_gate, size_t gate_samples, size_t slide_samples) {
  if (slide_samples > 0) {
    // Start from what the parameters were on the last block (a slide still running
    // continues from where it is).
    std::copy(&lock_layer_[track][0], &lock_layer_[track][0] + kNumLockable,
              &slide_from_[track][0]);
    slide_total_[track] = slide_left_[track] = slide_samples;
  } else {
    slide_left_[track] = 0;
  }
  uint32_t mask = 0;
  for (int p = 0; p < kNumLockable; ++p) {
    active_locks_[track][p] = locks ? locks[p] : NAN;
    if (locks && !std::isnan(locks[p])) {
      mask |= 1u << p;
    }
  }
  active_lock_mask[track].store(mask, std::memory_order_relaxed);
  float choke_value = project_->sounds[track].params[P_CHOKE];
  if (!std::isnan(active_locks_[track][P_CHOKE])) {
    choke_value = active_locks_[track][P_CHOKE];
  }
  int group = Discrete(choke_value, kNumChokeGroups);
  if (group > 0) {
    for (int other = 0; other < kNumTracks; ++other) {
      if (other != track &&
          Discrete(project_->sounds[other].params[P_CHOKE], kNumChokeGroups) == group) {
        voices_[other].Choke();
        for (int k = 0; k < kChordVoices; ++k) {
          if (chord_owner_[k] == other) {
            chord_voices_[k].Choke();
          }
        }
      }
    }
  }
  voices_[track].Trigger(velocity, note, max_gate, gate_samples);
  mods_[track].Trigger();
  trigger_count[track].fetch_add(1, std::memory_order_relaxed);
  last_trigger_sample[track] = sample_counter_;
  last_trigger_velocity[track].store(velocity, std::memory_order_relaxed);
}

void Engine::FireChord(int track, float velocity, int note, const int8_t* extras, int count,
                       size_t max_gate, size_t gate_samples) {
  for (int i = 0; i < count; ++i) {
    int voice = 0;
    for (int k = 0; k < kChordVoices; ++k) {
      if (!chord_voices_[k].busy()) {
        voice = k;
        break;
      }
      if (chord_started_[k] < chord_started_[voice]) {
        voice = k;
      }
    }
    chord_owner_[voice] = track;
    chord_started_[voice] = sample_counter_ + 1;  // Nonzero, and later than idle voices.
    chord_voices_[voice].Trigger(velocity, note + extras[i], max_gate, gate_samples);
  }
}

int Engine::chord_voices_active() const {
  int active = 0;
  for (int k = 0; k < kChordVoices; ++k) {
    active += chord_voices_[k].busy() ? 1 : 0;
  }
  return active;
}

void Engine::FireStep(int track, const Step& step, double step_duration) {
  size_t max_gate = SamplesPerStep() * 16;
  const size_t sub = static_cast<size_t>(step_duration / step.ratchets);
  if (step.ratchets > 1) {
    max_gate = sub > kBlockSize * 2 ? sub - kBlockSize : kBlockSize;
  }
  // A step's own gate length, in steps of a ratchet: it replaces the Gate parameter
  // (and may tie over the next steps when there is no ratchet).
  size_t gate_samples = 0;
  if (step.gate > 0.0f) {
    gate_samples = std::max(kBlockSize, static_cast<size_t>(step.gate * sub));
    gate_samples = std::min(gate_samples, max_gate);
  }
  size_t slide_samples = step.slide ? static_cast<size_t>(step_duration) : 0;
  FireTrack(track, step.velocity, step.note, step.locks, max_gate, gate_samples,
            slide_samples);
  if (step.chord_size > 0) {
    FireChord(track, step.velocity, step.note, step.chord, step.chord_size, max_gate,
              gate_samples);
  }
}

void Engine::ProcessWindow(double start, double end, bool allow_pattern_change) {
  double a = start;
  while (true) {
    const Pattern& pattern = project_->patterns[current_pattern_];
    int64_t boundary = pattern_start_ + pattern.length;
    double b = end;
    bool crossing = allow_pattern_change && end > static_cast<double>(boundary);
    if (crossing) {
      b = static_cast<double>(boundary);
    }

    // Events of the current pattern in [a, b).
    const double swing_offset = 2.0 * pattern.swing - 1.0;
    const double step_samples = static_cast<double>(SamplesPerStep());
    // Steps can be late (swing, micro timing, ratchets) or early (micro
    // timing), hence the margins.
    for (int64_t n = static_cast<int64_t>(std::floor(a)) - 2;
         n <= static_cast<int64_t>(std::floor(b)) + 1; ++n) {
      if (n < pattern_start_ || a >= b) {
        continue;
      }
      int64_t k = n - pattern_start_;
      double t0 = static_cast<double>(n) + ((k & 1) ? swing_offset : 0.0);
      bool base_in_window = t0 >= a && t0 < b;

      if (base_in_window) {
        pattern_step = static_cast<int>(k % pattern.length);
      }

      for (int t = 0; t < kNumTracks; ++t) {
        int s = TrackStep(pattern, t, n);
        if (base_in_window) {
          track_step[t] = s;
        }
        const Step& stored = pattern.tracks[t].steps[s];
        Step varied;
        if (!TrackAudible(t)) {
          continue;
        }
        if (pattern.vary > 0.0f) {
          if (!VaryStep(pattern, t, s, stored, &varied)) {
            continue;
          }
        } else if (!stored.on) {
          continue;
        }
        const Step& step = pattern.vary > 0.0f ? varied : stored;
        if (!RollProbability(n, t, step.probability)) {
          continue;
        }
        for (int r = 0; r < step.ratchets; ++r) {
          double time = t0 + step.micro + static_cast<double>(r) / step.ratchets;
          if (time < a || time >= b) {
            continue;
          }
          if (r == 0 && skip_step_[t] == n) {
            skip_step_[t] = -1;
            continue;
          }
          FireStep(t, step, step_samples);
        }
      }

      // Grids plays tracks A-C on top of the sequence.
      if (grids_enabled && base_in_window && n != last_grids_step_) {
        last_grids_step_ = n;
        const float* as = project_->assign;
        uint8_t density[kGridsParts];
        for (int i = 0; i < kGridsParts; ++i) {
          density[i] = static_cast<uint8_t>(as[A_GRIDS_DENSITY_1 + i] * 255.0f);
        }
        uint8_t accents = 0;
        uint8_t bits = grids_.EvaluateSixteenth(
            static_cast<int>(k % 16),
            static_cast<uint8_t>(grids_x * 255.0f),
            static_cast<uint8_t>(grids_y * 255.0f),
            density,
            static_cast<uint8_t>(as[A_GRIDS_CHAOS] * 255.0f),
            &accents);
        for (int part = 0; part < kGridsParts; ++part) {
          int s = TrackStep(pattern, part, n);
          if ((bits & (1 << part)) && TrackAudible(part) &&
              !pattern.tracks[part].steps[s].on) {
            FireTrack(part, (accents & (1 << part)) ? 1.0f : 0.7f, 0, nullptr,
                      SamplesPerStep() * 16);
          }
        }
      }
    }

    if (!crossing) {
      break;
    }
    // End of the pattern: chain to the next one.
    pattern_start_ = boundary;
    pattern_started_ = true;
    int previous_pattern = current_pattern_;
    if (queued_pattern_ >= 0) {
      ChangePattern(queued_pattern_);
      queued_pattern_ = -1;
    } else if (song_mode_ && project_->song_length > 0) {
      song_position_ = (song_position_ + 1) % project_->song_length;
      ChangePattern(project_->song[song_position_]);
    }
    if (current_pattern_ != previous_pattern) {
      poly_origin_ = boundary;
    }
    a = static_cast<double>(boundary);
  }
}

void Engine::RunSequencer(double start, double end) {
  bool loop_requested = xy_mode == XY_LOOPER && xy_touched;
  if (loop_requested) {
    double length = kLoopLengths[Discrete(xy_y, kNumLoopLengths)] / StepLength();
    if (!looping_) {
      double relative = start - static_cast<double>(pattern_start_);
      loop_start_ = static_cast<double>(pattern_start_) +
          std::floor(relative / length) * length;
    }
    looping_ = true;
    loop_length_ = length;
  } else {
    looping_ = false;
  }

  if (!looping_) {
    ProcessWindow(start, end, true);
    return;
  }
  double offset = std::fmod(start - loop_start_, loop_length_);
  if (offset < 0.0) {
    offset += loop_length_;
  }
  double a = loop_start_ + offset;
  double b = a + (end - start);
  double loop_end = loop_start_ + loop_length_;
  if (b <= loop_end) {
    ProcessWindow(a, b, false);
  } else {
    ProcessWindow(a, loop_end, false);
    ProcessWindow(loop_start_, loop_start_ + (b - loop_end), false);
  }
}

float Engine::LfoValue(int track, int lfo) const {
  const float* params = project_->sounds[track].params;
  int base = lfo == 0 ? P_LFO1_RATE : P_LFO2_RATE;
  double period = kLfoPeriods[Discrete(params[base], kNumLfoRates)] * 4.0;
  double position = (playing_ ? beat_position_ : free_position_) / period;
  double cycle = std::floor(position);
  float phase = static_cast<float>(position - cycle);
  switch (Discrete(params[base + 1], LFO_SHAPE_LAST)) {
    case LFO_SINE:
      return sinf(phase * 6.2831853f);
    case LFO_TRIANGLE:
      return 1.0f - 4.0f * std::fabs(phase - 0.5f);
    case LFO_SAW_UP:
      return 2.0f * phase - 1.0f;
    case LFO_SAW_DOWN:
      return 1.0f - 2.0f * phase;
    case LFO_SQUARE:
      return phase < 0.5f ? 1.0f : -1.0f;
    case LFO_SAMPLE_HOLD:
    default: {
      uint32_t h = Hash(static_cast<uint64_t>(cycle) * 64 + track * 2 + lfo);
      return static_cast<float>(h) / 2147483648.0f - 1.0f;
    }
  }
}

void Engine::ComputeParams(int track, float* params) {
  const float* base = project_->sounds[track].params;
  std::copy(base, base + kNumTrackParams, params);
  for (int p = 0; p < kNumLockable; ++p) {
    if (!std::isnan(active_locks_[track][p])) {
      params[p] = active_locks_[track][p];
    }
  }
  // Slide: move linearly from the previous values to the base and locks. The
  // choices (engine, filter mode, choke group) jump.
  if (slide_left_[track] > 0) {
    float t = 1.0f - static_cast<float>(slide_left_[track]) /
                         static_cast<float>(slide_total_[track]);
    for (int p = 0; p < kNumLockable; ++p) {
      float from = slide_from_[track][p];
      if (p != P_ENGINE && p != P_FILTER_MODE && p != P_CHOKE && !std::isnan(from)) {
        params[p] = from + (params[p] - from) * t;
      }
    }
    slide_left_[track] -= std::min(slide_left_[track], kBlockSize);
  }
  std::copy(params, params + kNumLockable, &lock_layer_[track][0]);
  for (int lfo = 0; lfo < 2; ++lfo) {
    int base_param = lfo == 0 ? P_LFO1_RATE : P_LFO2_RATE;
    int destination = DestinationToParam(params[base_param + 3]);
    if (destination >= 0) {
      float depth = params[base_param + 2] * 2.0f - 1.0f;
      params[destination] += depth * LfoValue(track, lfo) * 0.5f;
    }
  }
  // Peaks envelope and bouncing ball, both unipolar: full amount sweeps the
  // whole range of the destination.
  const struct { int amount; int dest; float value; } kSources[2] = {
    { P_ENV_AMOUNT, P_ENV_DEST, mods_[track].envelope() },
    { P_BALL_AMOUNT, P_BALL_DEST, mods_[track].ball() },
  };
  for (const auto& source : kSources) {
    int destination = DestinationToParam(params[source.dest]);
    if (destination >= 0) {
      params[destination] += (params[source.amount] * 2.0f - 1.0f) * source.value;
    }
  }
  if (xy_mode == XY_MACROS && xy_touched && track == selected_track) {
    const float* a = project_->assign;
    int x_destination = DestinationToParam(a[A_MACRO_X_DEST]);
    int y_destination = DestinationToParam(a[A_MACRO_Y_DEST]);
    if (x_destination >= 0) {
      params[x_destination] += (a[A_MACRO_X_DEPTH] * 2.0f - 1.0f) * xy_x;
    }
    if (y_destination >= 0) {
      params[y_destination] += (a[A_MACRO_Y_DEPTH] * 2.0f - 1.0f) * xy_y;
    }
  }
  for (int p = 0; p < kNumLockable; ++p) {
    params[p] = Clamp01(params[p]);
  }
}

void Engine::RenderClick(float* out_l, float* out_r) {
  if (click_envelope_ < 0.001f) {
    return;
  }
  const float increment = click_frequency_ / kSampleRate;
  for (size_t i = 0; i < kBlockSize; ++i) {
    click_phase_ += increment;
    if (click_phase_ >= 1.0f) {
      click_phase_ -= 1.0f;
    }
    float s = sinf(click_phase_ * 6.2831853f) * click_envelope_ * 0.25f;
    click_envelope_ *= 0.997f;
    out_l[i] += s;
    out_r[i] += s;
  }
}

void Engine::RenderBlock(float* out_l, float* out_r) {
  // Positions: steps for the sequencer, 16th notes for everything musical.
  const double sixteenths = tempo() / 60.0 * 4.0 / kSampleRate * kBlockSize;
  const double step_length = StepLength();
  const double start = position_;
  double end = start + sixteenths / step_length;
  if (playing_ && project_->midi_clock_sync) {
    // Follow the external clock (24 PPQN, 6 ticks per 16th): run at its
    // tempo but never past the next tick, and catch up when a tick is ahead.
    double ticks_per_step = 6.0 * step_length;
    double tick_position = clock_ticks_ / ticks_per_step;
    double next_tick = (clock_ticks_ + 1) / ticks_per_step;
    end = std::clamp(end, std::max(start, tick_position), std::max(start, next_tick));
  }
  const double beat_start = beat_position_;
  const double beat_end = beat_start + (end - start) * step_length;
  if (playing_) {
    RunSequencer(start, end);
    // Metronome on quarter notes, higher on the first beat of each bar.
    double next_beat = std::ceil(beat_start / 4.0) * 4.0;
    if (metronome && next_beat < beat_end) {
      click_envelope_ = 1.0f;
      click_frequency_ = std::fmod(next_beat, 16.0) == 0.0 ? 1600.0f : 1000.0f;
    }
  }

  // Note repeat, on the grid of the running clock.
  if (repeat_mask_) {
    double clock = playing_ ? beat_start : free_position_;
    double period = kRepeatPeriods[repeat_rate_];
    double next = std::ceil(clock / period) * period;
    if (next < clock + sixteenths) {
      size_t gate = static_cast<size_t>(period * SamplesPerSixteenth());
      gate = gate > kBlockSize * 2 ? gate - kBlockSize : kBlockSize;
      // When recording, repeats are written into the pattern: one
      // hit per step, or ratchets when they are faster than the steps.
      int ratchets = std::clamp(static_cast<int>(std::lround(step_length / period)), 1,
                                kMaxRatchets);
      for (int t = 0; t < kNumTracks; ++t) {
        if (repeat_mask_ & (1 << t)) {
          FireTrack(t, 0.8f, 0, nullptr, gate);
          RecordHit(t, 0.8f, 0, ratchets);
        }
      }
    }
  }
  if (playing_) {
    position_ = end;
    beat_position_ = beat_end;
  }
  free_position_ += sixteenths;

  buses_.Clear();
  float extra_delay_send = 0.0f;
  float dj_filter = 0.0f;
  if (xy_mode == XY_FX && xy_touched) {
    dj_filter = xy_x * 2.0f - 1.0f;
    extra_delay_send = xy_y;
  }
  float params[kNumTrackParams];
  for (int t = 0; t < kNumTracks; ++t) {
    const float* base = project_->sounds[t].params;
    mods_[t].Process(base[P_ENV_ATTACK], base[P_ENV_DECAY],
                     base[P_BALL_GRAVITY], base[P_BALL_BOUNCE]);
    ComputeParams(t, params);
    for (int p = 0; p < kNumLockable; ++p) {
      effective_param[t][p].store(params[p], std::memory_order_relaxed);
    }
    const SampleData* sample = VoiceSample(params);
    const SampleSlices* slices = sample ? &project_->slices[SampleSlot(params[P_AUX])] : nullptr;
    voices_[t].Render(params, extra_delay_send, &buses_, sample, slices);
    for (int k = 0; k < kChordVoices; ++k) {
      if (chord_owner_[k] == t && chord_voices_[k].busy()) {
        chord_voices_[k].Render(params, extra_delay_send, &buses_, sample, slices);
      }
    }
    track_peak[t].store(voices_[t].peak(), std::memory_order_relaxed);
    track_gate[t].store(voices_[t].gate_high(), std::memory_order_relaxed);
  }
  master_fx_->Process(project_->master, tempo(), project_->volume,
                      dj_filter, &buses_, out_l, out_r);
  clouds_fx_->Process(out_l, out_r, kBlockSize, project_->clouds, clouds_freeze);
  RecordResample(out_l, out_r);
  RenderClick(out_l, out_r);

  float peak = master_peak.load(std::memory_order_relaxed) * 0.98f;
  int w = scope_write.load(std::memory_order_relaxed);
  for (size_t i = 0; i < kBlockSize; ++i) {
    float mono = 0.5f * (out_l[i] + out_r[i]);
    peak = std::max(peak, std::max(std::fabs(out_l[i]), std::fabs(out_r[i])));
    scope[w].store(mono, std::memory_order_relaxed);
    w = (w + 1) % kScopeSize;
  }
  scope_write.store(w, std::memory_order_relaxed);
  master_peak.store(peak, std::memory_order_relaxed);
  sample_counter_ += kBlockSize;
}

void Engine::Render(float* out, size_t frames) {
  std::lock_guard<std::mutex> lock(mutex_);
#if defined(__SSE__) || defined(__x86_64__)
  // Flush denormals to zero (FTZ + DAZ), as on the module's ARM core: decaying
  // reverb, filter and envelope tails otherwise make x86 rendering 10-100x
  // slower in bursts, and the audio device underruns.
  _mm_setcsr(_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
  // Same on ARM64: set FPCR.FZ.
  uint64_t fpcr;
  __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
  __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
#endif
  auto t0 = std::chrono::steady_clock::now();
  for (size_t i = 0; i < frames; ++i) {
    if (block_read_ >= kBlockSize) {
      RenderBlock(block_l_, block_r_);
      block_read_ = 0;
    }
    out[2 * i] = block_l_[block_read_];
    out[2 * i + 1] = block_r_[block_read_];
    ++block_read_;
  }
  auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0);
  float budget = static_cast<float>(frames) / kSampleRate;
  if (budget > 0.0f) {
    float load = elapsed.count() / budget;
    float smoothed = cpu_load.load(std::memory_order_relaxed);
    cpu_load.store(smoothed + (load - smoothed) * 0.1f, std::memory_order_relaxed);
  }
}

}  // namespace drumfake
