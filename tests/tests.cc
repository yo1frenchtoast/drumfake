// Tests for the Drumf(re)ake core: sequencer timing, Grids port, persistence and
// a full render. Run: ./build/drumfreake_tests

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include "audio/engine.h"
#include "audio/wav_render.h"
#include "core/grids_gen.h"
#include "core/midi_parser.h"
#include "core/presets.h"
#include "core/project_io.h"
#include "core/scales.h"
#include "grids/resources.h"
#include <nlohmann/json.hpp>

using namespace drumfreake;

namespace {

int failures = 0;
int checks = 0;

#define CHECK(condition) \
  do { \
    ++checks; \
    if (!(condition)) { \
      ++failures; \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
    } \
  } while (0)

#define CHECK_NEAR(a, b, tolerance) \
  do { \
    ++checks; \
    double a_ = (a); \
    double b_ = (b); \
    if (std::fabs(a_ - b_) > (tolerance)) { \
      ++failures; \
      fprintf(stderr, "%s:%d: CHECK_NEAR failed: %s = %g, expected %g +/- %g\n", \
              __FILE__, __LINE__, #a, a_, b_, static_cast<double>(tolerance)); \
    } \
  } while (0)

// An engine with an empty pattern 1 at 120 BPM (6000 samples per step).
std::unique_ptr<Engine> MakeEngine() {
  auto engine = std::make_unique<Engine>();
  engine->Init();
  engine->project().patterns[0].Clear();
  engine->project().bpm = 120.0f;
  return engine;
}

const int64_t kSamplesPerStep = 6000;

// Renders `frames` frames and returns the sample times of every trigger of
// `track`.
std::vector<int64_t> RenderAndCollect(Engine* engine, int track, size_t frames) {
  std::vector<int64_t> triggers;
  std::vector<float> buffer(2 * kBlockSize);
  uint32_t seen = engine->trigger_count[track].load();
  for (size_t done = 0; done < frames; done += kBlockSize) {
    engine->Render(buffer.data(), kBlockSize);
    uint32_t count = engine->trigger_count[track].load();
    if (count != seen) {
      seen = count;
      triggers.push_back(engine->last_trigger_sample[track].load());
    }
  }
  return triggers;
}

void TestParams() {
  CHECK(kSampleRate == plaits::kSampleRate);
  CHECK(Discrete(0.0f, 24) == 0);
  CHECK(Discrete(1.0f, 24) == 23);
  for (int i = 0; i < kNumEngines; ++i) {
    CHECK(Discrete(DiscreteToNormalized(i, kNumEngines), kNumEngines) == i);
  }
  CHECK(DestinationToParam(0.0f) == -1);
  CHECK(DestinationToParam(DiscreteToNormalized(1 + P_TIMBRE, kNumDestinations)) == P_TIMBRE);
  CHECK(FormatValue(FORMAT_ENGINE, DiscreteToNormalized(21, kNumEngines)) == "Bass Drum");
  CHECK(FormatValue(FORMAT_NOTE, NoteToNormalizedPitch(60.0f)) == "C4");
  CHECK(FormatValue(FORMAT_CRUSH, 0.0f) == "Off");
  CHECK_NEAR(CutoffToHz(0.0f), 20.0, 1e-3);
  CHECK_NEAR(CutoffToHz(1.0f), 20480.0, 1e-1);
}

void TestScales() {
  CHECK(MajorScaleDegree(36, 36) == 0);
  CHECK(MajorScaleDegree(36, 41) == 3);
  CHECK(MajorScaleDegree(36, 48) == 7);
  CHECK(MajorScaleDegree(36, 37) == -1);
  CHECK(MajorScaleDegree(36, 50) == -1);  // Past the eighth note.
  // Pads 1-8 cover the first 8 degrees, continuing into the next octave.
  const int kMajor[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };
  const int kPentatonicMinor[8] = { 0, 3, 5, 7, 10, 12, 15, 17 };
  for (int pad = 0; pad < 8; ++pad) {
    CHECK(KeyOffset(0, 0, 0, pad) == kMajor[pad]);
    CHECK(KeyOffset(0, 8, 0, pad) == kPentatonicMinor[pad]);
    CHECK(KeyOffset(0, 10, 0, pad) == pad);  // Chromatic.
  }
  // Root and octave transpose everything.
  CHECK(KeyOffset(2, 1, -1, 0) == 2 - 12);  // D minor, one octave down.
  CHECK(KeyOffset(2, 1, 0, 2) == 2 + 3);    // Its minor third.
  CHECK(NoteName(60) == "C4");
  CHECK(NoteName(61) == "C#4");
  CHECK(NoteName(33) == "A1");
  CHECK(std::string(scale(kNumScales - 1).name) == "Chromatic");
}

void TestGrids() {
  // At the corner of a map cell, the interpolation returns the node values.
  for (int step = 0; step < kGridsStepsPerBar; ++step) {
    for (int part = 0; part < kGridsParts; ++part) {
      uint8_t expected = grids::node_10[part * kGridsStepsPerBar + step];
      uint8_t value = GridsGenerator::ReadDrumMap(step, part, 0, 0);
      // Each U8Mix loses up to 1 LSB, like avrlib's version on the module.
      CHECK_NEAR(value, expected, 2.0);
    }
  }
  // Center of the map: node_4.
  CHECK_NEAR(GridsGenerator::ReadDrumMap(0, 0, 128, 128), grids::node_4[0], 2.0);

  // Zero density fires nothing; full density fires the downbeat kick.
  GridsGenerator generator;
  generator.Init(1);
  uint8_t full[kGridsParts] = { 255, 255, 255 };
  uint8_t none[kGridsParts] = { 0, 0, 0 };
  uint8_t accents = 0;
  CHECK(generator.Evaluate(0, 128, 128, none, 0, &accents) == 0);
  uint8_t bits = generator.Evaluate(0, 128, 128, full, 0, &accents);
  CHECK(bits & 1);
}

void TestStepTiming() {
  auto engine = MakeEngine();
  Pattern& p = engine->project().patterns[0];
  p.tracks[0].steps[0].on = true;
  p.tracks[0].steps[4].on = true;
  engine->Play();
  // Stop just before the pattern loops.
  std::vector<int64_t> triggers = RenderAndCollect(engine.get(), 0, 16 * kSamplesPerStep - 100);
  CHECK(triggers.size() == 2);
  if (triggers.size() == 2) {
    CHECK_NEAR(triggers[0], 0, kBlockSize);
    CHECK_NEAR(triggers[1], 4 * kSamplesPerStep, kBlockSize);
  }
}

void TestSwing() {
  auto engine = MakeEngine();
  Pattern& p = engine->project().patterns[0];
  p.swing = 0.75f;  // Odd steps are delayed by half a step.
  p.tracks[0].steps[5].on = true;
  engine->Play();
  std::vector<int64_t> triggers = RenderAndCollect(engine.get(), 0, 16 * kSamplesPerStep);
  CHECK(triggers.size() == 1);
  if (!triggers.empty()) {
    CHECK_NEAR(triggers[0], 5.5 * kSamplesPerStep, kBlockSize);
  }
}

void TestRatchetsAndProbability() {
  auto engine = MakeEngine();
  Pattern& p = engine->project().patterns[0];
  p.tracks[0].steps[2].on = true;
  p.tracks[0].steps[2].ratchets = 4;
  p.tracks[1].steps[0].on = true;
  p.tracks[1].steps[0].probability = 0.0f;
  engine->Play();
  std::vector<int64_t> triggers = RenderAndCollect(engine.get(), 0, 16 * kSamplesPerStep);
  CHECK(triggers.size() == 4);
  for (size_t i = 0; i < triggers.size(); ++i) {
    CHECK_NEAR(triggers[i], 2.0 * kSamplesPerStep + i * kSamplesPerStep / 4.0, kBlockSize);
  }
  CHECK(engine->trigger_count[1].load() == 0);
}

void TestTrackLengthAndPatternChain() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.patterns[0].length = 8;
  project.patterns[0].tracks[0].steps[0].on = true;
  project.patterns[0].tracks[1].length = 3;  // Polymeter.
  project.patterns[0].tracks[1].steps[0].on = true;
  project.patterns[1].Clear();
  project.patterns[1].tracks[2].steps[0].on = true;
  engine->Play();
  engine->SelectPattern(1);  // Queued until the end of pattern 1.
  CHECK(engine->current_pattern() == 0);
  std::vector<int64_t> kicks = RenderAndCollect(engine.get(), 0, 8 * kSamplesPerStep - 100);
  CHECK(kicks.size() == 1);
  // Track B loops every 3 steps: steps 0, 3 and 6 of the 8-step pattern.
  CHECK(engine->trigger_count[1].load() == 3);
  std::vector<int64_t> hats = RenderAndCollect(engine.get(), 2, 2 * kSamplesPerStep);
  CHECK(engine->current_pattern() == 1);
  CHECK(hats.size() == 1);
  if (!hats.empty()) {
    CHECK_NEAR(hats[0], 8 * kSamplesPerStep, kBlockSize);
  }
}

// Track A: 3 steps with a hit on step 1, in a 4-step pattern.
std::unique_ptr<Engine> MakePolyEngine(bool poly) {
  auto engine = MakeEngine();
  Pattern& p = engine->project().patterns[0];
  p.length = 4;
  p.poly = poly;
  p.tracks[0].length = 3;
  p.tracks[0].steps[0].on = true;
  return engine;
}

std::vector<int64_t> TriggerSteps(const std::vector<int64_t>& samples) {
  std::vector<int64_t> steps;
  for (int64_t s : samples) {
    steps.push_back((s + kSamplesPerStep / 2) / kSamplesPerStep);
  }
  return steps;
}

void TestPolyrhythm() {
  // Without poly, track A restarts with each 4-step pattern loop.
  auto engine = MakePolyEngine(false);
  engine->Play();
  std::vector<int64_t> steps = TriggerSteps(
      RenderAndCollect(engine.get(), 0, 12 * kSamplesPerStep - 100));
  CHECK((steps == std::vector<int64_t>{ 0, 3, 4, 7, 8, 11 }));

  // With poly, it keeps cycling every 3 steps across pattern loops.
  engine = MakePolyEngine(true);
  engine->Play();
  steps = TriggerSteps(RenderAndCollect(engine.get(), 0, 12 * kSamplesPerStep - 100));
  CHECK((steps == std::vector<int64_t>{ 0, 3, 6, 9 }));

  // Play realigns every track (sample times count from engine start, so
  // compare the offsets from the restart).
  engine->Stop();
  engine->Play();
  steps = TriggerSteps(RenderAndCollect(engine.get(), 0, 4 * kSamplesPerStep - 100));
  CHECK(steps.size() == 2);
  if (steps.size() == 2) {
    CHECK(steps[0] == 12);  // First block after the 12 steps rendered above.
    CHECK(steps[1] - steps[0] == 3);
  }

  // A pattern change realigns too: pattern 2 starts at step 4, so track A
  // plays on 4 and 7 instead of continuing on 6.
  engine = MakePolyEngine(true);
  engine->project().patterns[1] = engine->project().patterns[0];
  engine->Play();
  engine->SelectPattern(1);
  steps = TriggerSteps(RenderAndCollect(engine.get(), 0, 8 * kSamplesPerStep - 100));
  CHECK((steps == std::vector<int64_t>{ 0, 3, 4, 7 }));

  // A track longer than the pattern plays its extra steps in poly mode.
  engine = MakeEngine();
  Pattern& p = engine->project().patterns[0];
  p.length = 4;
  p.poly = true;
  p.tracks[1].length = 6;
  p.tracks[1].steps[5].on = true;
  engine->Play();
  steps = TriggerSteps(RenderAndCollect(engine.get(), 1, 12 * kSamplesPerStep - 100));
  CHECK((steps == std::vector<int64_t>{ 5, 11 }));
}

void TestLockFeedback() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.sounds[0].params[P_TIMBRE] = 0.2f;
  project.patterns[0].tracks[0].steps[0].on = true;
  project.patterns[0].tracks[0].steps[0].locks[P_TIMBRE] = 0.9f;
  project.patterns[0].tracks[0].steps[2].on = true;  // No lock.
  // LFO 1 on track B's morph: square, full depth.
  project.sounds[1].params[P_LFO1_SHAPE] = DiscreteToNormalized(LFO_SQUARE, LFO_SHAPE_LAST);
  project.sounds[1].params[P_LFO1_DEPTH] = 1.0f;
  project.sounds[1].params[P_LFO1_DEST] = DiscreteToNormalized(1 + P_MORPH, kNumDestinations);
  engine->Play();
  RenderAndCollect(engine.get(), 0, kSamplesPerStep);
  CHECK(engine->active_lock_mask[0].load() == (1u << P_TIMBRE));
  CHECK_NEAR(engine->effective_param[0][P_TIMBRE].load(), 0.9, 1e-6);
  CHECK_NEAR(engine->effective_param[0][P_HARMONICS].load(),
             project.sounds[0].params[P_HARMONICS], 1e-6);
  CHECK(std::fabs(engine->effective_param[1][P_MORPH].load() -
                  project.sounds[1].params[P_MORPH]) > 0.1);
  // The next note has no lock: back to the knob value.
  RenderAndCollect(engine.get(), 0, 2 * kSamplesPerStep);
  CHECK(engine->active_lock_mask[0].load() == 0);
  CHECK_NEAR(engine->effective_param[0][P_TIMBRE].load(), 0.2, 1e-6);
}

// Patterns 1-3 are 4 steps long; pattern N plays track N on its first step.
std::unique_ptr<Engine> MakeSongEngine() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  for (int i = 0; i < 3; ++i) {
    project.patterns[i].Clear();
    project.patterns[i].length = 4;
    project.patterns[i].tracks[i].steps[0].on = true;
  }
  const int kChain[] = { 0, 0, 1, 2 };
  project.song_length = 4;
  for (int i = 0; i < 4; ++i) {
    project.song[i] = kChain[i];
  }
  return engine;
}

void TestSong() {
  // Chain 1-1-2-3, looped: pattern 1 plays twice, then 2, then 3.
  auto engine = MakeSongEngine();
  engine->SetSongMode(true);
  CHECK(engine->song_mode());
  engine->Play();
  std::vector<int64_t> a = TriggerSteps(RenderAndCollect(engine.get(), 0, 24 * kSamplesPerStep - 100));
  CHECK((a == std::vector<int64_t>{ 0, 4, 16, 20 }));
  CHECK(engine->trigger_count[1].load() == 1);  // Step 8.
  CHECK(engine->trigger_count[2].load() == 1);  // Step 12.
  // Steps 20-23: second entry of the chain again (pattern 1).
  CHECK(engine->current_pattern() == 0);
  CHECK(engine->song_position() == 1);

  // Play restarts from the top of the chain.
  engine->Stop();
  engine->Play();
  CHECK(engine->current_pattern() == 0);
  CHECK(engine->song_position() == 0);

  // Selecting a pattern leaves song mode (it plays after the current one).
  engine->SelectPattern(2);
  CHECK(!engine->song_mode());
  RenderAndCollect(engine.get(), 0, 4 * kSamplesPerStep + 100);
  CHECK(engine->current_pattern() == 2);

  // Enabling the song while playing starts the chain at the next pattern.
  engine = MakeSongEngine();
  engine->SelectPattern(2);
  engine->Play();
  engine->SetSongMode(true);
  CHECK(engine->current_pattern() == 2);
  RenderAndCollect(engine.get(), 0, 4 * kSamplesPerStep + 100);
  CHECK(engine->current_pattern() == 0);
  CHECK(engine->song_position() == 0);

  // An empty chain can't be played.
  engine = MakeEngine();
  engine->SetSongMode(true);
  CHECK(!engine->song_mode());
}

void TestMidiParser() {
  MidiParser parser;
  std::vector<MidiMessage> messages;
  auto collect = [&](const MidiMessage& m) { messages.push_back(m); };
  // Note on, then running status with a clock byte in the middle of it, a
  // SysEx to skip, then a Song Position Pointer.
  const uint8_t bytes[] = {
    0x99, 36, 100,
    38, 0xf8, 90,
    0xf0, 0x7e, 0x01, 0xf7,
    0xf2, 0x10, 0x01,
    0xfa,
  };
  parser.Parse(bytes, sizeof(bytes), collect);
  CHECK(messages.size() == 5);
  if (messages.size() == 5) {
    CHECK(messages[0].type() == MIDI_NOTE_ON && messages[0].channel() == 9);
    CHECK(messages[0].data[0] == 36 && messages[0].data[1] == 100);
    CHECK(messages[1].type() == MIDI_CLOCK);
    CHECK(messages[2].type() == MIDI_NOTE_ON && messages[2].data[0] == 38 &&
          messages[2].data[1] == 90);
    CHECK(messages[3].type() == MIDI_SONG_POSITION);
    CHECK((messages[3].data[0] | (messages[3].data[1] << 7)) == 144);
    CHECK(messages[4].type() == MIDI_START);
  }
}

MidiMessage Midi(uint8_t status, uint8_t data0 = 0, uint8_t data1 = 0) {
  MidiMessage m;
  m.status = status;
  m.data[0] = data0;
  m.data[1] = data1;
  m.size = status >= 0xf8 ? 1 : 3;
  return m;
}

void TestMidiNotes() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  // Track A (channel 1) plays the note received: C4 on a C2 track is +24.
  project.sounds[0].params[P_PITCH] = NoteToNormalizedPitch(36.0f);
  engine->recording = true;
  engine->Play();
  engine->HandleMidi(Midi(0x90, 60, 127), 0.0);
  CHECK(engine->trigger_count[0].load() == 1);
  CHECK(project.patterns[0].tracks[0].steps[0].on);
  CHECK(project.patterns[0].tracks[0].steps[0].note == 24);
  engine->Stop();
  engine->recording = false;

  // Channel 8 is track H; velocity 0 is a note off.
  engine->HandleMidi(Midi(0x97, 50, 64), 0.0);
  engine->HandleMidi(Midi(0x97, 51, 0), 0.0);
  CHECK(engine->trigger_count[7].load() == 1);

  // Channel 10 belongs to no track: the white keys from C2 trigger A-H.
  const int kWhiteKeys[kNumTracks] = { 36, 38, 40, 41, 43, 45, 47, 48 };
  for (int t = 0; t < kNumTracks; ++t) {
    engine->HandleMidi(Midi(0x99, kWhiteKeys[t], 100), 0.0);
  }
  engine->HandleMidi(Midi(0x99, 37, 100), 0.0);  // C#2: not in the scale.
  CHECK(engine->trigger_count[0].load() == 2);
  CHECK(engine->trigger_count[1].load() == 1);
  CHECK(engine->trigger_count[7].load() == 2);

  // Two tracks on the same channel play together; channel 0 turns one off.
  project.midi_channels[1] = 1;
  project.midi_channels[7] = 0;
  engine->HandleMidi(Midi(0x90, 60, 100), 0.0);
  engine->HandleMidi(Midi(0x97, 60, 100), 0.0);  // Channel 8: now unassigned.
  CHECK(engine->trigger_count[0].load() == 3);
  CHECK(engine->trigger_count[1].load() == 2);
  CHECK(engine->trigger_count[7].load() == 2);  // 60 is not a trigger note.
  CHECK(engine->midi_activity.load() == 14);
}

void TestMidiClock() {
  // A 100 BPM clock: 24 ticks per quarter note, 7200 samples per step.
  auto engine = MakeEngine();
  engine->project().midi_clock_sync = true;
  Pattern& p = engine->project().patterns[0];
  p.tracks[0].steps[0].on = true;
  p.tracks[0].steps[4].on = true;
  const double kTick = 60.0 / 100.0 / 24.0;
  const size_t kSamplesPerTick = 1200;  // At 48 kHz.
  std::vector<float> buffer(2 * kSamplesPerTick);

  double time = 0.0;
  for (int i = 0; i < 24; ++i) {  // Clock running before Start.
    engine->HandleMidi(Midi(MIDI_CLOCK), time);
    time += kTick;
  }
  CHECK_NEAR(engine->tempo(), 100.0, 0.5);
  CHECK_NEAR(engine->project().bpm, 120.0, 1e-6);  // The saved tempo is kept.

  engine->HandleMidi(Midi(MIDI_START), time);
  CHECK(engine->playing());
  // Nothing moves before the first tick.
  engine->Render(buffer.data(), kSamplesPerTick);
  CHECK(engine->trigger_count[0].load() == 0);

  std::vector<int64_t> ticks_at_trigger;
  uint32_t seen = 0;
  for (int tick = 0; tick < 6 * 8; ++tick) {  // Two beats.
    engine->HandleMidi(Midi(MIDI_CLOCK), time);
    time += kTick;
    engine->Render(buffer.data(), kSamplesPerTick);
    uint32_t count = engine->trigger_count[0].load();
    if (count != seen) {
      seen = count;
      ticks_at_trigger.push_back(tick);
    }
  }
  // Steps 1 and 5 play on ticks 0 and 24.
  CHECK((ticks_at_trigger == std::vector<int64_t>{ 0, 24 }));

  // Without ticks, playback waits instead of running ahead.
  int step_before = engine->pattern_step.load();
  std::vector<float> long_buffer(2 * 20 * kSamplesPerTick);
  engine->Render(long_buffer.data(), 20 * kSamplesPerTick);
  CHECK(engine->pattern_step.load() == step_before);

  engine->HandleMidi(Midi(MIDI_STOP), time);
  CHECK(!engine->playing());
  // Song Position: step 4 (MIDI beat 4), then Continue plays it on the
  // next tick.
  engine->HandleMidi(Midi(MIDI_SONG_POSITION, 4, 0), time);
  engine->HandleMidi(Midi(MIDI_CONTINUE), time);
  CHECK(engine->playing());
  uint32_t before = engine->trigger_count[0].load();
  engine->HandleMidi(Midi(MIDI_CLOCK), time);
  engine->Render(buffer.data(), kSamplesPerTick);
  CHECK(engine->trigger_count[0].load() == before + 1);

  // Transport messages are ignored when not following the clock.
  engine->project().midi_clock_sync = false;
  engine->HandleMidi(Midi(MIDI_STOP), time);
  CHECK(engine->playing());
  CHECK_NEAR(engine->tempo(), 120.0, 1e-6);
}

void TestKits() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.patterns[1] = project.patterns[0];
  // Pattern 2 gets its own kick.
  ApplyPreset(PRESET_KICK_909, &project.patterns[1].kit[0]);

  engine->SelectPattern(1);
  CHECK(project.sounds[0].name == "Kick 909");
  // An edit belongs to the current pattern and comes back with it.
  project.sounds[0].params[P_DECAY] = 0.9f;
  engine->SelectPattern(0);
  CHECK(project.sounds[0].name == "Kick 808");
  engine->SelectPattern(1);
  CHECK_NEAR(project.sounds[0].params[P_DECAY], 0.9, 1e-6);

  // The kit also changes at the end of a pattern when switching while playing.
  engine->Play();
  engine->SelectPattern(0);
  RenderAndCollect(engine.get(), 0, 16 * kSamplesPerStep + 100);
  CHECK(engine->current_pattern() == 0);
  CHECK(project.sounds[0].name == "Kick 808");
  engine->Stop();

  // Panel: the sound playing stays across pattern changes, and its edits
  // are stored nowhere until StoreKit().
  engine->SetPanel(true);
  project.sounds[0].params[P_TIMBRE] = 0.123f;
  engine->SelectPattern(1);
  CHECK(project.sounds[0].name == "Kick 808");
  CHECK_NEAR(project.sounds[0].params[P_TIMBRE], 0.123, 1e-6);
  CHECK(project.patterns[1].kit[0].name == "Kick 909");
  // Leaving Panel mode reloads the current pattern's kit.
  engine->SetPanel(false);
  CHECK(project.sounds[0].name == "Kick 909");
  // Storing writes the panel kit into the current pattern.
  engine->SetPanel(true);
  engine->SelectPattern(0);
  ApplyPreset(PRESET_RIM, &project.sounds[0]);
  engine->StoreKit();
  engine->SetPanel(false);
  CHECK(project.sounds[0].name == "Rim");
  CHECK(project.patterns[0].kit[0].name == "Rim");
}

void TestLegacyProjectKits() {
  // A version 1 project: one kit for the project, none in the patterns.
  auto project = std::make_unique<Project>();
  project->Init();
  ApplyPreset(PRESET_CLAP, &project->sounds[3]);
  std::string path = (std::filesystem::temp_directory_path() / "drumfreake_v1.json").string();
  std::string error;
  CHECK(SaveProject(*project, path, &error));
  nlohmann::json j;
  {
    std::ifstream in(path);
    in >> j;
  }
  for (auto& pattern : j["patterns"]) {
    pattern.erase("kit");
  }
  {
    std::ofstream out(path);
    out << j.dump();
  }
  auto loaded = std::make_unique<Project>();
  CHECK(LoadProject(path, loaded.get(), &error));
  std::filesystem::remove(path);
  for (const Pattern& pattern : loaded->patterns) {
    CHECK(pattern.kit[3].name == "Clap");
  }
}

void TestClearParamLocks() {
  TrackSequence track;
  track.Clear();
  track.steps[1].locks[P_CUTOFF] = 0.2f;
  track.steps[5].locks[P_CUTOFF] = 0.4f;
  track.steps[5].locks[P_DECAY] = 0.6f;
  track.steps[5].on = true;
  CHECK(track.ClearLocks(P_CUTOFF) == 2);
  CHECK(!track.steps[1].has_lock(P_CUTOFF));
  CHECK(!track.steps[5].has_lock(P_CUTOFF));
  CHECK(track.steps[5].has_lock(P_DECAY));  // Other parameters stay.
  CHECK(track.steps[5].on);                 // And so do the steps.
  CHECK(track.ClearLocks(P_CUTOFF) == 0);
}

void TestCopyPages() {
  auto project = std::make_unique<Project>();
  project->Init();
  Pattern& p = project->patterns[0];  // The 16-step demo.
  // A then D: the 16 steps repeat up to step 64, locks included.
  p.CopyPages(0, 3);
  CHECK(p.length == 64);
  for (int t = 0; t < kNumTracks; ++t) {
    for (int s = 16; s < 64; ++s) {
      CHECK(p.tracks[t].steps[s].on == p.tracks[t].steps[s % 16].on);
    }
  }
  CHECK(p.tracks[7].steps[8 + 48].has_lock(P_TIMBRE));
  // B then D: steps 1-32 repeat once.
  p.tracks[0].steps[20].on = !p.tracks[0].steps[20].on;
  p.CopyPages(1, 3);
  CHECK(p.tracks[0].steps[52].on == p.tracks[0].steps[20].on);
  CHECK(p.tracks[0].steps[36].on == p.tracks[0].steps[4].on);
  // D then A: one page onto an earlier one; the length does not change.
  p.tracks[1].steps[48].on = true;
  p.tracks[1].steps[0].on = false;
  p.CopyPages(3, 0);
  CHECK(p.tracks[1].steps[0].on);
  CHECK(p.length == 64);
}

void TestLiveRecording() {
  auto engine = MakeEngine();
  engine->recording = true;
  engine->Play();
  // Just before step 3: rounds to step 3, which must then not play twice.
  RenderAndCollect(engine.get(), 5, 3 * kSamplesPerStep - 1200);
  engine->LiveTrigger(5, 0.9f, 2);
  const Step& step = engine->project().patterns[0].tracks[5].steps[3];
  CHECK(step.on);
  CHECK_NEAR(step.velocity, 0.9, 1e-6);
  CHECK(step.note == 2);
  uint32_t before = engine->trigger_count[5].load();
  RenderAndCollect(engine.get(), 5, 2 * kSamplesPerStep);
  CHECK(engine->trigger_count[5].load() == before);
}

void TestChoke() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  ApplyPreset(PRESET_CLOSED_HAT, &project.sounds[2]);
  ApplyPreset(PRESET_OPEN_HAT, &project.sounds[3]);
  project.patterns[0].tracks[3].steps[0].on = true;
  engine->Play();
  RenderAndCollect(engine.get(), 3, kSamplesPerStep / 2);
  float open_peak = engine->track_peak[3].load();
  CHECK(open_peak > 0.01f);
  engine->LiveTrigger(2, 0.8f, 0);  // The closed hat chokes the open hat.
  RenderAndCollect(engine.get(), 3, kSamplesPerStep);
  CHECK(engine->track_peak[3].load() < open_peak * 0.3f);
}

void TestProjectRoundTrip() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.Init();
  project.bpm = 97.5f;
  project.patterns[3].tracks[4].steps[17].on = true;
  project.patterns[3].tracks[4].steps[17].ratchets = 3;
  project.patterns[3].tracks[4].steps[17].locks[P_CUTOFF] = 0.25f;
  project.sounds[6].params[P_MORPH] = 0.125f;
  ApplyPreset(PRESET_SPEECH, &project.patterns[5].kit[2]);
  project.chords[2] = 0x42;
  project.patterns[3].poly = true;
  project.midi_clock_sync = true;
  project.midi_channels[2] = 10;
  project.midi_trigger_note = 48;
  project.key_root = 9;
  project.key_scale = 8;
  project.key_octave = -2;
  project.song_length = 3;
  project.song[0] = 2;
  project.song[1] = 2;
  project.song[2] = 5;
  project.patterns[3].tracks[4].length = 5;

  std::string path = (std::filesystem::temp_directory_path() / "drumfreake_test.json").string();
  std::string error;
  CHECK(SaveProject(project, path, &error));
  auto loaded = std::make_unique<Project>();
  CHECK(LoadProject(path, loaded.get(), &error));
  std::filesystem::remove(path);

  CHECK_NEAR(loaded->bpm, 97.5, 1e-4);
  const Step& step = loaded->patterns[3].tracks[4].steps[17];
  CHECK(step.on);
  CHECK(step.ratchets == 3);
  CHECK_NEAR(step.locks[P_CUTOFF], 0.25, 1e-6);
  CHECK(!step.has_lock(P_TIMBRE));
  CHECK_NEAR(loaded->sounds[6].params[P_MORPH], 0.125, 1e-6);
  CHECK(loaded->patterns[5].kit[2].name == "Speech");
  CHECK(loaded->patterns[4].kit[2].name == project.patterns[4].kit[2].name);
  CHECK(loaded->sounds[0].name == project.sounds[0].name);
  CHECK(loaded->chords[2] == 0x42);
  CHECK(loaded->patterns[3].poly);
  CHECK(loaded->midi_clock_sync);
  CHECK(loaded->midi_channels[2] == 10);
  CHECK(loaded->midi_channels[3] == 4);
  CHECK(loaded->midi_trigger_note == 48);
  CHECK(loaded->key_root == 9);
  CHECK(loaded->key_scale == 8);
  CHECK(loaded->key_octave == -2);
  CHECK(loaded->song_length == 3);
  CHECK(loaded->song[0] == 2 && loaded->song[1] == 2 && loaded->song[2] == 5);
  CHECK(!loaded->patterns[0].poly);
  CHECK(loaded->patterns[3].tracks[4].length == 5);
  CHECK(loaded->patterns[0].tracks[7].steps[8].has_lock(P_TIMBRE));
}

void TestGridsPlayback() {
  auto engine = MakeEngine();
  engine->grids_enabled = true;
  engine->grids_x = 0.5f;
  engine->grids_y = 0.5f;
  engine->Play();
  RenderAndCollect(engine.get(), 0, 16 * kSamplesPerStep - 100);
  // Grids plays tracks A-C only, on an empty pattern.
  CHECK(engine->trigger_count[0].load() > 0);
  CHECK(engine->trigger_count[1].load() > 0);
  CHECK(engine->trigger_count[2].load() > 0);
  CHECK(engine->trigger_count[3].load() == 0);
  // Baking writes the same bar into the pattern.
  engine->BakeGrids();
  CHECK(engine->project().patterns[0].tracks[0].steps[0].on);
  CHECK(!engine->project().patterns[0].tracks[0].IsEmpty());
}

void TestFullRender() {
  auto engine = std::make_unique<Engine>();
  engine->Init();  // Default kit and demo pattern.
  engine->project().master[M_CHORUS] = 0.5f;
  engine->project().sounds[1].params[P_REVERB_SEND] = 0.5f;
  engine->project().sounds[2].params[P_DELAY_SEND] = 0.5f;
  RenderStats stats;
  std::string error;
  CHECK(RenderBars(engine.get(), 2, "", &stats, &error));
  CHECK(stats.non_finite == 0);
  CHECK(stats.peak > 0.1f);
  CHECK(stats.peak <= 1.0f);
  for (int t = 0; t < kNumTracks; ++t) {
    CHECK(stats.track_peak[t] > 0.005f);
  }
  // Every Plaits engine renders finite, audible audio.
  for (int e = 0; e < kNumEngines; ++e) {
    auto single = MakeEngine();
    single->project().sounds[0].params[P_ENGINE] = DiscreteToNormalized(e, kNumEngines);
    single->project().patterns[0].tracks[0].steps[0].on = true;
    RenderStats s;
    CHECK(RenderBars(single.get(), 1, "", &s, &error));
    CHECK(s.non_finite == 0);
    if (s.peak < 0.001f) {
      fprintf(stderr, "engine %d (%s) is silent\n", e, kEngineNames[e]);
    }
    CHECK(s.peak > 0.001f);
  }
}

}  // namespace

int main() {
  TestParams();
  TestScales();
  TestGrids();
  TestStepTiming();
  TestSwing();
  TestRatchetsAndProbability();
  TestTrackLengthAndPatternChain();
  TestPolyrhythm();
  TestLockFeedback();
  TestSong();
  TestMidiParser();
  TestMidiNotes();
  TestMidiClock();
  TestKits();
  TestClearParamLocks();
  TestCopyPages();
  TestLegacyProjectKits();
  TestLiveRecording();
  TestChoke();
  TestGridsPlayback();
  TestProjectRoundTrip();
  TestFullRender();
  printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
