#include "audio/engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#if defined(__SSE__) || defined(__x86_64__)
#include <xmmintrin.h>
#endif

namespace drumfreake {

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
      master_fx_(new MasterFx()) {
}

Engine::~Engine() { }

void Engine::Init() {
  project_->Init();
  for (int i = 0; i < kNumTracks; ++i) {
    voices_[i].Init();
    track_step[i] = -1;
    trigger_count[i] = 0;
    track_peak[i] = 0.0f;
    last_trigger_sample[i] = -1;
    active_lock_mask[i] = 0;
    skip_step_[i] = -1;
    for (int p = 0; p < kNumLockable; ++p) {
      active_locks_[i][p] = NAN;
    }
  }
  for (int i = 0; i < kNumTracks; ++i) {
    for (int p = 0; p < kNumLockable; ++p) {
      effective_param[i][p] = project_->sounds[i].params[p];
    }
  }
  master_fx_->Init();
  grids_.Init(0x5eed);
  master_peak = 0.0f;
  pattern_step = -1;
  for (int i = 0; i < kScopeSize; ++i) {
    scope[i] = 0.0f;
  }
  scope_write = 0;
  cpu_load = 0.0f;

  playing_ = false;
  song_mode_ = false;
  song_position_ = 0;
  current_pattern_ = 0;
  queued_pattern_ = -1;
  position_ = 0.0;
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
    current_pattern_ = project_->song[0];
    queued_pattern_ = -1;
  }
  if (queued_pattern_ >= 0) {
    current_pattern_ = queued_pattern_;
    queued_pattern_ = -1;
  }
  playing_ = true;
  position_ = 0.0;
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
    current_pattern_ = project_->song[0];
  }
}

void Engine::SelectPattern(int pattern) {
  song_mode_ = false;
  if (!playing_) {
    current_pattern_ = pattern;
    queued_pattern_ = -1;
  } else {
    queued_pattern_ = pattern == current_pattern_ ? -1 : pattern;
  }
}

void Engine::SetRepeat(uint8_t mask, int rate) {
  repeat_mask_ = mask;
  repeat_rate_ = std::max(0, std::min(kNumRepeatRates - 1, rate));
}

size_t Engine::SamplesPerStep() const {
  return static_cast<size_t>(kSampleRate * 15.0f / project_->bpm);
}

bool Engine::RollProbability(int64_t step, int track, float probability) const {
  if (probability >= 1.0f) {
    return true;
  }
  uint32_t h = Hash(static_cast<uint64_t>(step) * 16 + track);
  return static_cast<float>(h) / 4294967296.0f < probability;
}

int Engine::TrackStep(const Pattern& pattern, int track, int64_t n) const {
  // In poly mode, tracks only realign on Play and on pattern changes; they
  // keep cycling on their own length when the pattern loops.
  int64_t position = pattern.poly
      ? n - poly_origin_
      : PositiveModulo(n - pattern_start_, pattern.length);
  return static_cast<int>(PositiveModulo(position, pattern.track_length(track)));
}

void Engine::LiveTrigger(int track, float velocity, int note_offset) {
  FireTrack(track, velocity, note_offset, nullptr, SamplesPerStep() * 16);
  if (!recording || !playing_ || looping_) {
    return;
  }
  Pattern& pattern = project_->patterns[current_pattern_];
  int64_t n = static_cast<int64_t>(std::llround(position_));
  Step& step = pattern.tracks[track].steps[TrackStep(pattern, track, n)];
  step.on = true;
  step.velocity = velocity;
  step.note = note_offset;
  step.ratchets = 1;
  step.probability = 1.0f;
  if (static_cast<double>(n) > position_) {
    // Rounded to an upcoming step: don't play it twice.
    skip_step_[track] = n;
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

void Engine::FireTrack(int track, float velocity, int note, const float* locks,
                       size_t max_gate) {
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
      }
    }
  }
  voices_[track].Trigger(velocity, note, max_gate);
  trigger_count[track].fetch_add(1, std::memory_order_relaxed);
  last_trigger_sample[track] = sample_counter_;
}

void Engine::FireStep(int track, const Step& step, double step_duration) {
  size_t max_gate = SamplesPerStep() * 16;
  if (step.ratchets > 1) {
    size_t sub = static_cast<size_t>(step_duration / step.ratchets);
    max_gate = sub > kBlockSize * 2 ? sub - kBlockSize : kBlockSize;
  }
  FireTrack(track, step.velocity, step.note, step.locks, max_gate);
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
    for (int64_t n = static_cast<int64_t>(std::floor(a)) - 2;
         n <= static_cast<int64_t>(std::floor(b)); ++n) {
      if (n < pattern_start_ || a >= b) {
        continue;
      }
      int64_t k = n - pattern_start_;
      double t0 = static_cast<double>(n) + ((k & 1) ? swing_offset : 0.0);
      bool base_in_window = t0 >= a && t0 < b;

      if (base_in_window) {
        pattern_step = static_cast<int>(k % pattern.length);
        if (metronome && k % 4 == 0) {
          click_envelope_ = 1.0f;
          click_frequency_ = (k % pattern.length) == 0 ? 1600.0f : 1000.0f;
        }
      }

      for (int t = 0; t < kNumTracks; ++t) {
        int s = TrackStep(pattern, t, n);
        if (base_in_window) {
          track_step[t] = s;
        }
        const Step& step = pattern.tracks[t].steps[s];
        if (!step.on || muted[t]) {
          continue;
        }
        if (!RollProbability(n, t, step.probability)) {
          continue;
        }
        for (int r = 0; r < step.ratchets; ++r) {
          double time = t0 + static_cast<double>(r) / step.ratchets;
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
          if ((bits & (1 << part)) && !muted[part] &&
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
    int previous_pattern = current_pattern_;
    if (queued_pattern_ >= 0) {
      current_pattern_ = queued_pattern_;
      queued_pattern_ = -1;
    } else if (song_mode_ && project_->song_length > 0) {
      song_position_ = (song_position_ + 1) % project_->song_length;
      current_pattern_ = project_->song[song_position_];
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
    double length = kLoopLengths[Discrete(xy_y, kNumLoopLengths)];
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
  double position = (playing_ ? position_ : free_position_) / period;
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
  for (int lfo = 0; lfo < 2; ++lfo) {
    int base_param = lfo == 0 ? P_LFO1_RATE : P_LFO2_RATE;
    int destination = DestinationToParam(params[base_param + 3]);
    if (destination >= 0) {
      float depth = params[base_param + 2] * 2.0f - 1.0f;
      params[destination] += depth * LfoValue(track, lfo) * 0.5f;
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
  const double increment = project_->bpm / 60.0 * 4.0 / kSampleRate * kBlockSize;
  const double start = position_;
  if (playing_) {
    RunSequencer(start, start + increment);
  }

  // Note repeat, on the grid of the running clock.
  if (repeat_mask_) {
    double clock = playing_ ? start : free_position_;
    double period = kRepeatPeriods[repeat_rate_];
    double next = std::ceil(clock / period) * period;
    if (next < clock + increment) {
      size_t gate = static_cast<size_t>(period * SamplesPerStep());
      gate = gate > kBlockSize * 2 ? gate - kBlockSize : kBlockSize;
      for (int t = 0; t < kNumTracks; ++t) {
        if (repeat_mask_ & (1 << t)) {
          FireTrack(t, 0.8f, 0, nullptr, gate);
        }
      }
    }
  }
  if (playing_) {
    position_ = start + increment;
  }
  free_position_ += increment;

  buses_.Clear();
  float extra_delay_send = 0.0f;
  float dj_filter = 0.0f;
  if (xy_mode == XY_FX && xy_touched) {
    dj_filter = xy_x * 2.0f - 1.0f;
    extra_delay_send = xy_y;
  }
  float params[kNumTrackParams];
  for (int t = 0; t < kNumTracks; ++t) {
    ComputeParams(t, params);
    for (int p = 0; p < kNumLockable; ++p) {
      effective_param[t][p].store(params[p], std::memory_order_relaxed);
    }
    voices_[t].Render(params, extra_delay_send, &buses_);
    track_peak[t].store(voices_[t].peak(), std::memory_order_relaxed);
  }
  master_fx_->Process(project_->master, project_->bpm, project_->volume,
                      dj_filter, &buses_, out_l, out_r);
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

}  // namespace drumfreake
