// Tests for the Drumfake core: sequencer timing, Grids port, persistence and
// a full render. Run: ./build/drumfake_tests

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

using namespace drumfake;

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
    CHECK(KeyOffset(0, kChromaticScale, 0, pad) == kMajor[pad]);  // The piano's white keys.
  }
  // Black keys: a semitone above the pad, when the next pad is at least a tone away.
  const int kBlack[8] = { 1, 3, -1, 6, 8, 10, -1, 13 };
  for (int pad = 0; pad < 8; ++pad) {
    int offset = 0;
    bool exists = KeySharpOffset(0, kChromaticScale, 0, pad, &offset);
    CHECK(exists == (kBlack[pad] >= 0));
    if (exists) {
      CHECK(offset == kBlack[pad]);
    }
    CHECK(KeySharpOffset(5, 8, -1, pad, &offset));  // Pentatonic minor: gaps all >= 2.
    CHECK(offset == 5 - 12 + kPentatonicMinor[pad] + 1);
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

// Every trigger of `track` over `frames`: sample time and velocity.
std::vector<std::pair<int64_t, float>> RenderAndCollectVelocities(Engine* engine, int track,
                                                                  size_t frames) {
  std::vector<std::pair<int64_t, float>> triggers;
  std::vector<float> buffer(2 * kBlockSize);
  uint32_t seen = engine->trigger_count[track].load();
  for (size_t done = 0; done < frames; done += kBlockSize) {
    engine->Render(buffer.data(), kBlockSize);
    uint32_t count = engine->trigger_count[track].load();
    if (count != seen) {
      seen = count;
      triggers.push_back({ engine->last_trigger_sample[track].load(),
                           engine->last_trigger_velocity[track].load() });
    }
  }
  return triggers;
}

void TestVary() {
  // A 16-step hat on every step and a kick on every beat (velocity 0.8).
  auto make = [](float vary, uint32_t seed) {
    auto engine = MakeEngine();
    Pattern& p = engine->project().patterns[0];
    for (int s = 0; s < 16; ++s) {
      p.tracks[2].steps[s].on = true;
      p.tracks[0].steps[s].on = s % 4 == 0;
    }
    p.vary = vary;
    p.vary_seed = seed;
    engine->Play();
    return engine;
  };
  const size_t kLoop = 16 * kSamplesPerStep;

  // No Vary: every step plays at its velocity, on time.
  {
    auto engine = make(0.0f, 1);
    auto hats = RenderAndCollectVelocities(engine.get(), 2, kLoop - 100);
    CHECK(hats.size() == 16);
    for (size_t i = 0; i < hats.size(); ++i) {
      CHECK_NEAR(hats[i].first, static_cast<double>(i) * kSamplesPerStep, kBlockSize);
      CHECK_NEAR(hats[i].second, 0.8, 1e-6);
    }
  }

  // Full Vary: hits are dropped, softened and moved, the same way on every loop.
  auto engine = make(1.0f, 1234);
  const Step stored = engine->project().patterns[0].tracks[2].steps[5];
  auto first = RenderAndCollectVelocities(engine.get(), 2, kLoop);
  auto second = RenderAndCollectVelocities(engine.get(), 2, kLoop);
  CHECK(first.size() == second.size());
  CHECK(first.size() < 16 + 4);  // At most a ratchet on a few steps.
  bool softer = false;
  for (size_t i = 0; i < first.size() && i < second.size(); ++i) {
    CHECK_NEAR(second[i].first - first[i].first, static_cast<double>(kLoop), kBlockSize);
    CHECK_NEAR(second[i].second, first[i].second, 1e-6);
    CHECK(first[i].second <= 0.8f + 1e-6f);
    softer = softer || first[i].second < 0.7f;
  }
  CHECK(softer);
  // Nothing is written into the pattern.
  const Step& after = engine->project().patterns[0].tracks[2].steps[5];
  CHECK(after.on == stored.on && after.velocity == stored.velocity);
  CHECK(after.micro == stored.micro && after.ratchets == stored.ratchets);
  CHECK(!after.has_lock(P_DECAY));

  // Dice: a new draw gives other variations.
  engine->DiceVary();
  CHECK(engine->project().patterns[0].vary_seed != 1234u);
  auto third = RenderAndCollectVelocities(engine.get(), 2, kLoop);
  bool different = third.size() != second.size();
  for (size_t i = 0; !different && i < third.size(); ++i) {
    different = std::fabs(third[i].second - second[i].second) > 1e-6f ||
        std::llabs(third[i].first - second[i].first - static_cast<int64_t>(kLoop)) > kBlockSize;
  }
  CHECK(different);

  // The same seed gives the same variations; an empty track gets no ghost notes.
  auto again = make(1.0f, 1234);
  auto replay = RenderAndCollectVelocities(again.get(), 2, kLoop);
  CHECK(replay.size() == first.size());
  for (size_t i = 0; i < replay.size() && i < first.size(); ++i) {
    CHECK(replay[i].first == first[i].first);
  }
  CHECK(again->trigger_count[5].load() == 0);

  // Ghost notes: with a sparse kick and full Vary, some seeds add soft hits on the steps
  // between the beats (ratchets added by Vary fall half a step after a beat).
  int ghosts = 0;
  for (uint32_t seed = 1; seed <= 20; ++seed) {
    auto sparse = make(1.0f, seed);
    for (auto& [time, velocity] : RenderAndCollectVelocities(sparse.get(), 0, kLoop)) {
      int64_t offset = time % (4 * kSamplesPerStep);
      if (offset > kSamplesPerStep * 3 / 4 && offset < kSamplesPerStep * 13 / 4) {
        ++ghosts;
        CHECK(velocity <= 0.45f * 0.8f + 1e-6f);
      }
    }
  }
  CHECK(ghosts > 0);
}

void TestPatternExtend() {
  Pattern p;
  p.Clear();
  p.tracks[0].steps[0].on = true;
  p.tracks[0].steps[0].locks[P_CUTOFF] = 0.3f;
  p.tracks[0].steps[10].on = true;
  p.tracks[2].steps[3].on = true;
  // Stale steps past the length are overwritten by the repetition.
  p.tracks[0].steps[20].on = true;

  // 16 > 32: the whole pattern repeats.
  p.Extend(-1, 32);
  CHECK(p.length == 32);
  CHECK(p.tracks[0].steps[16].on && p.tracks[0].steps[26].on);
  CHECK_NEAR(p.tracks[0].steps[16].locks[P_CUTOFF], 0.3, 1e-6);
  CHECK(!p.tracks[0].steps[20].on);
  CHECK(p.tracks[2].steps[19].on);

  // A length that is not a whole page repeats on its own period: 12 > 48.
  p.Clear();
  p.length = 12;
  p.tracks[1].steps[5].on = true;
  p.Extend(-1, 48);
  CHECK(p.length == 48);
  for (int s = 0; s < 48; ++s) {
    CHECK(p.tracks[1].steps[s].on == (s % 12 == 5));
  }

  // Shorter: only the length changes.
  p.Extend(-1, 16);
  CHECK(p.length == 16);
  CHECK(p.tracks[1].steps[41].on);

  // One track (Poly, Steps view): only that track grows, to its own length.
  p.Clear();
  p.poly = true;
  p.tracks[0].steps[2].on = true;
  p.tracks[3].steps[2].on = true;
  p.Extend(3, 64);
  CHECK(p.length == 16);
  CHECK(p.tracks[3].length == 64);
  CHECK(p.tracks[3].steps[50].on && p.tracks[3].steps[34].on);
  CHECK(!p.tracks[0].steps[18].on);
  CHECK(p.tracks[0].length == 0);

  // The whole pattern (Poly, Patterns view): tracks with their own length keep it.
  p.tracks[5].length = 6;
  p.tracks[5].steps[1].on = true;
  p.Extend(-1, 32);
  CHECK(p.length == 32);
  CHECK(p.tracks[0].steps[18].on);
  CHECK(p.tracks[3].length == 64);
  CHECK(p.tracks[5].length == 6);
  CHECK(!p.tracks[5].steps[7].on);
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

// Number of samples the gate of `track` stays high, from the first block of a note.
int64_t MeasureGate(Engine* engine, int track, size_t frames) {
  std::vector<float> buffer(2 * kBlockSize);
  int64_t high = 0;
  for (size_t done = 0; done < frames; done += kBlockSize) {
    engine->Render(buffer.data(), kBlockSize);
    if (engine->track_gate[track].load()) {
      high += kBlockSize;
    }
  }
  return high;
}

// Chords built on the scale of the Keys mode, and their names.
void TestChordNotes() {
  int notes[kMaxChordNotes];
  // C major (the piano): C, Dm, Bdim, then the other kinds on C.
  CHECK(ChordNotes(0, kChromaticScale, 0, 0, 0, notes) == 3);
  CHECK(notes[0] == 48 && notes[1] == 52 && notes[2] == 55);
  CHECK(ChordName(notes, 3) == "C");
  ChordNotes(0, kChromaticScale, 0, 1, 0, notes);
  CHECK(ChordName(notes, 3) == "Dm");
  ChordNotes(0, kChromaticScale, 0, 6, 0, notes);
  CHECK(ChordName(notes, 3) == "Bdim");
  ChordNotes(0, 0, 0, 7, 0, notes);  // Pad 8: the root an octave up.
  CHECK(notes[0] == 60 && ChordName(notes, 3) == "C");
  const char* const kOnC[kNumChordShapes] = {
    "C", "Cmaj7", "Csus2", "Csus4", "Cadd9", "C6", "Cmaj9", "C5"
  };
  for (int shape = 0; shape < kNumChordShapes; ++shape) {
    int count = ChordNotes(0, 0, 0, 0, shape, notes);
    CHECK(count == 1 + chord_shape(shape).size && count <= kMaxChordNotes);
    CHECK(ChordName(notes, count) == kOnC[shape]);
  }
  // Root and octave move the whole chord.
  ChordNotes(2, 0, -1, 0, 1, notes);  // D, one octave down.
  CHECK(notes[0] == 48 + 2 - 12);
  // Another scale: the chords come from its notes (A minor: Am, Bdim, C...).
  ChordNotes(9, 1, 0, 0, 0, notes);
  CHECK(ChordName(notes, 3) == "Am");
  ChordNotes(9, 1, 0, 1, 0, notes);
  CHECK(ChordName(notes, 3) == "Bdim");
  ChordNotes(9, 1, 0, 2, 1, notes);
  CHECK(ChordName(notes, 4) == "Cmaj7");
}

// A chord step plays its extra notes on the voices of the pool.
void TestChordPlayback() {
  auto engine = MakeEngine();
  Step& step = engine->project().patterns[0].tracks[0].steps[0];
  step.on = true;
  step.chord_size = 2;
  step.chord[0] = 4;
  step.chord[1] = 7;
  engine->Play();
  std::vector<float> buffer(2 * kBlockSize);
  int most = 0;
  for (size_t done = 0; done < kSamplesPerStep; done += kBlockSize) {
    engine->Render(buffer.data(), kBlockSize);
    most = std::max(most, engine->chord_voices_active());
  }
  CHECK(most == 2);
  // A step without a chord does not use the pool (after the tail died out).
  auto single = MakeEngine();
  single->project().patterns[0].tracks[0].steps[0].on = true;
  single->Play();
  int used = 0;
  for (size_t done = 0; done < kSamplesPerStep; done += kBlockSize) {
    single->Render(buffer.data(), kBlockSize);
    used = std::max(used, single->chord_voices_active());
  }
  CHECK(used == 0);
  // A chord makes more sound than its root alone.
  auto rms = [&](bool with_chord) {
    auto e = MakeEngine();
    Step& s = e->project().patterns[0].tracks[0].steps[0];
    s.on = true;
    if (with_chord) {
      s.chord_size = 2;
      s.chord[0] = 4;
      s.chord[1] = 7;
    }
    e->Play();
    double sum = 0.0;
    for (size_t done = 0; done < kSamplesPerStep; done += kBlockSize) {
      e->Render(buffer.data(), kBlockSize);
      for (float v : buffer) {
        CHECK(std::isfinite(v));
        sum += static_cast<double>(v) * v;
      }
    }
    return sum;
  };
  CHECK(rms(true) > 1.3 * rms(false));
}

// A chord played live is written into the step, and a single note clears it.
void TestChordRecording() {
  auto engine = MakeEngine();
  engine->recording = true;
  engine->Play();
  std::vector<float> buffer(2 * kBlockSize);
  engine->Render(buffer.data(), kBlockSize);
  const int8_t extras[2] = { 3, 7 };
  engine->LiveChord(0, 0.8f, 5, extras, 2);
  const Step& step = engine->project().patterns[0].tracks[0].steps[0];
  CHECK(step.on && step.note == 5);
  CHECK(step.chord_size == 2 && step.chord[0] == 3 && step.chord[1] == 7);
  engine->LiveTrigger(0, 0.8f, 2);
  CHECK(engine->project().patterns[0].tracks[0].steps[0].chord_size == 0);
}

// Snapshot: everything playable is saved on arming and comes back on restoring, whatever
// is done in between, and the patterns never see the edits.
void TestSnapshot() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.sounds[0].params[P_TIMBRE] = 0.3f;
  project.master[M_REVERB_LEVEL] = 0.4f;
  project.clouds[C_DRY_WET] = 0.6f;
  project.assign[A_MACRO_X_DEPTH] = 0.2f;
  project.bpm = 100.0f;
  project.volume = 0.5f;
  project.key_scale = 3;
  project.chord_type = 2;
  engine->muted[2] = true;
  engine->soloed[3] = true;
  engine->grids_x = 0.2f;
  engine->grids_enabled = true;
  CHECK(!engine->snapshot_armed());
  engine->ArmSnapshot();
  CHECK(engine->snapshot_armed() && engine->panel());
  // A drop: change everything, also the pattern.
  project.sounds[0].params[P_TIMBRE] = 0.9f;
  project.sounds[5].name = "Drop";
  project.master[M_REVERB_LEVEL] = 1.0f;
  project.clouds[C_DRY_WET] = 0.0f;
  project.assign[A_MACRO_X_DEPTH] = 0.9f;
  project.bpm = 140.0f;
  project.volume = 1.0f;
  project.key_scale = 7;
  project.chord_type = 5;
  engine->muted[2] = false;
  engine->muted[4] = true;
  engine->soloed[3] = false;
  engine->grids_x = 0.9f;
  engine->grids_enabled = false;
  engine->SelectPattern(1);
  project.sounds[1].params[P_MORPH] = 0.1f;
  // The edits did not reach the patterns.
  CHECK_NEAR(project.patterns[0].kit[0].params[P_TIMBRE], 0.3, 1e-6);
  const float pattern1_morph = project.patterns[1].kit[1].params[P_MORPH];
  CHECK(pattern1_morph != 0.1f);
  engine->RestoreSnapshot();
  CHECK(!engine->snapshot_armed() && !engine->panel());
  CHECK_NEAR(project.master[M_REVERB_LEVEL], 0.4, 1e-6);
  CHECK_NEAR(project.clouds[C_DRY_WET], 0.6, 1e-6);
  CHECK_NEAR(project.assign[A_MACRO_X_DEPTH], 0.2, 1e-6);
  CHECK_NEAR(project.bpm, 100.0, 1e-6);
  CHECK_NEAR(project.volume, 0.5, 1e-6);
  CHECK(project.key_scale == 3 && project.chord_type == 2);
  CHECK(engine->muted[2] && !engine->muted[4] && engine->soloed[3]);
  CHECK_NEAR(engine->grids_x, 0.2, 1e-6);
  CHECK(engine->grids_enabled);
  // The pattern changed meanwhile: its own kit is playing, untouched by the drop.
  CHECK(engine->current_pattern() == 1);
  CHECK_NEAR(project.sounds[1].params[P_MORPH], pattern1_morph, 1e-6);
  CHECK(project.sounds[5].name != "Drop");

  // On the same pattern: the sound is exactly as it was, even unsaved edits.
  auto same = MakeEngine();
  same->project().sounds[0].params[P_TIMBRE] = 0.3f;  // Not stored in the pattern.
  same->ArmSnapshot();
  same->project().sounds[0].params[P_TIMBRE] = 0.9f;
  same->RestoreSnapshot();
  CHECK_NEAR(same->project().sounds[0].params[P_TIMBRE], 0.3, 1e-6);
  CHECK_NEAR(same->project().patterns[0].kit[0].params[P_TIMBRE], 0.3, 1e-6);

  // With Panel mode on before, the sound playing comes back and Panel stays on.
  auto panel = MakeEngine();
  panel->SetPanel(true);
  panel->project().sounds[0].params[P_TIMBRE] = 0.3f;
  panel->ArmSnapshot();
  panel->project().sounds[0].params[P_TIMBRE] = 0.9f;
  panel->SelectPattern(1);
  panel->RestoreSnapshot();
  CHECK(panel->panel());
  CHECK_NEAR(panel->project().sounds[0].params[P_TIMBRE], 0.3, 1e-6);
  // Restoring when nothing is armed does nothing.
  panel->project().sounds[0].params[P_TIMBRE] = 0.7f;
  panel->RestoreSnapshot();
  CHECK_NEAR(panel->project().sounds[0].params[P_TIMBRE], 0.7, 1e-6);
}

// A step's own gate length replaces the Gate parameter, and can tie past a step.
void TestStepGate() {
  const int64_t kTolerance = 3 * kBlockSize;
  for (float gate : { 0.25f, 0.5f, 2.0f }) {
    auto engine = MakeEngine();
    Project& project = engine->project();
    project.sounds[0].params[P_GATE] = 0.0f;  // The knob alone gives about 1 ms.
    project.patterns[0].tracks[0].steps[0].on = true;
    project.patterns[0].tracks[0].steps[0].gate = gate;
    engine->Play();
    int64_t measured = MeasureGate(engine.get(), 0, 4 * kSamplesPerStep);
    CHECK_NEAR(measured, gate * kSamplesPerStep, kTolerance);
  }
  // Without a step gate, the Gate knob decides.
  auto engine = MakeEngine();
  engine->project().sounds[0].params[P_GATE] = 0.0f;
  engine->project().patterns[0].tracks[0].steps[0].on = true;
  engine->Play();
  CHECK(MeasureGate(engine.get(), 0, 4 * kSamplesPerStep) < kSamplesPerStep / 4);
  // With ratchets, the gate is a fraction of one ratchet.
  auto ratcheted = MakeEngine();
  Step& step = ratcheted->project().patterns[0].tracks[0].steps[0];
  step.on = true;
  step.ratchets = 2;
  step.gate = 0.5f;
  ratcheted->Play();
  int64_t measured = MeasureGate(ratcheted.get(), 0, kSamplesPerStep);
  CHECK_NEAR(measured, 2 * (0.5 * kSamplesPerStep / 2), 2 * kTolerance);
}

// A slide step glides from the previous values to its locks over the step, and
// the next step without slide jumps again.
void TestStepSlide() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.sounds[0].params[P_TIMBRE] = 0.2f;
  Pattern& pattern = project.patterns[0];
  pattern.tracks[0].steps[0].on = true;           // Base value.
  pattern.tracks[0].steps[2].on = true;
  pattern.tracks[0].steps[2].locks[P_TIMBRE] = 0.8f;
  pattern.tracks[0].steps[2].slide = true;
  pattern.tracks[0].steps[4].on = true;           // Lock without slide.
  pattern.tracks[0].steps[4].locks[P_TIMBRE] = 0.8f;
  engine->Play();
  RenderAndCollect(engine.get(), 0, 2 * kSamplesPerStep + kSamplesPerStep / 2);
  float middle = engine->effective_param[0][P_TIMBRE].load();
  CHECK(middle > 0.4f && middle < 0.6f);  // Halfway between 0.2 and 0.8.
  RenderAndCollect(engine.get(), 0, kSamplesPerStep / 2 - 4 * kBlockSize);
  CHECK_NEAR(engine->effective_param[0][P_TIMBRE].load(), 0.8, 0.05);
  RenderAndCollect(engine.get(), 0, 4 * kBlockSize + kBlockSize);
  // Step 5 locks the same value without sliding: no movement at all.
  RenderAndCollect(engine.get(), 0, kSamplesPerStep);
  CHECK_NEAR(engine->effective_param[0][P_TIMBRE].load(), 0.8, 1e-6);
  // A slide step without locks glides back to the knob.
  auto back = MakeEngine();
  back->project().sounds[0].params[P_TIMBRE] = 0.2f;
  Pattern& p = back->project().patterns[0];
  p.tracks[0].steps[0].on = true;
  p.tracks[0].steps[0].locks[P_TIMBRE] = 0.8f;
  p.tracks[0].steps[1].on = true;
  p.tracks[0].steps[1].slide = true;
  back->Play();
  RenderAndCollect(back.get(), 0, kSamplesPerStep + kSamplesPerStep / 2);
  float returning = back->effective_param[0][P_TIMBRE].load();
  CHECK(returning > 0.4f && returning < 0.6f);
  // The choices never glide: the engine jumps to its lock at once.
  auto jump = MakeEngine();
  jump->project().sounds[0].params[P_ENGINE] = DiscreteToNormalized(3, kNumEngines);
  Step& s = jump->project().patterns[0].tracks[0].steps[0];
  s.on = true;
  s.slide = true;
  s.locks[P_ENGINE] = DiscreteToNormalized(9, kNumEngines);
  jump->Play();
  RenderAndCollect(jump.get(), 0, kSamplesPerStep / 4);
  CHECK(Discrete(jump->effective_param[0][P_ENGINE].load(), kNumEngines) == 9);
}

// Peaks envelope and bouncing ball, restarted by every note.
void TestPeaksMods() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  float* params = project.sounds[0].params;
  params[P_MORPH] = 0.2f;
  params[P_ENV_DEST] = DiscreteToNormalized(1 + P_MORPH, kNumDestinations);
  params[P_ENV_AMOUNT] = 0.75f;  // +50%.
  params[P_TIMBRE] = 0.0f;
  params[P_BALL_DEST] = DiscreteToNormalized(1 + P_TIMBRE, kNumDestinations);
  params[P_BALL_AMOUNT] = 0.75f;
  params[P_BALL_GRAVITY] = 0.6f;
  params[P_BALL_BOUNCE] = 0.3f;  // Higher = more elastic.

  std::vector<float> buffer(2 * kBlockSize);
  auto block = [&]() { engine->Render(buffer.data(), kBlockSize); };

  // Nothing moves before the first note.
  for (int i = 0; i < 10; ++i) block();
  CHECK_NEAR(engine->effective_param[0][P_MORPH].load(), 0.2, 1e-6);
  CHECK_NEAR(engine->effective_param[0][P_TIMBRE].load(), 0.0, 1e-6);

  engine->LiveTrigger(0, 0.8f, 0);
  float max_morph = 0.0f;
  int bounces = 0;
  float max_timbre = 0.0f;
  float previous = 0.0f;
  bool falling = false;
  for (int i = 0; i < 48000 * 3 / static_cast<int>(kBlockSize); ++i) {
    block();
    float morph = engine->effective_param[0][P_MORPH].load();
    float timbre = engine->effective_param[0][P_TIMBRE].load();
    max_morph = std::max(max_morph, morph);
    max_timbre = std::max(max_timbre, timbre);
    if (timbre > previous + 1e-4f && falling) {
      ++bounces;  // Was falling, now rising: the ball hit the floor.
    }
    if (timbre < previous - 1e-4f) falling = true;
    else if (timbre > previous + 1e-4f) falling = false;
    previous = timbre;
  }
  CHECK(max_morph > 0.6f);            // Envelope peak: 0.2 + 0.5.
  CHECK(max_timbre > 0.4f);           // Ball starts at the top.
  CHECK(bounces >= 2);
  // The envelope decays (8 s max, 0.5 s default) and the ball comes to rest.
  CHECK_NEAR(engine->effective_param[0][P_MORPH].load(), 0.2, 0.02);
  CHECK(engine->effective_param[0][P_TIMBRE].load() < 0.05f);

  // A new note restarts both.
  engine->LiveTrigger(0, 0.8f, 0);
  for (int i = 0; i < 100; ++i) block();
  CHECK(engine->effective_param[0][P_MORPH].load() > 0.5f);
}

// Marbles rhythm generator: determinism, deja vu loops, Drums pattern tables.
void TestMarbles() {
  MarblesSettings settings;
  bool a[32], b[32], c[32], d[32];
  settings.model = 0;
  settings.deja_vu = 0.0f;
  MarblesGenerate(settings, 7, 32, a, b);
  MarblesGenerate(settings, 7, 32, c, d);
  CHECK(std::equal(a, a + 32, c) && std::equal(b, b + 32, d));  // Same seed.
  MarblesGenerate(settings, 8, 32, c, d);
  CHECK(!std::equal(a, a + 32, c));                              // New seed.
  // Complementary Bernoulli: each step goes to one output or the other.
  for (int i = 0; i < 32; ++i) CHECK(a[i] != b[i]);

  // Deja vu at 100%: the loop of 4 steps repeats.
  settings.deja_vu = 1.0f;
  settings.length = 4;
  MarblesGenerate(settings, 7, 32, a, b);
  for (int i = 4; i < 32; ++i) CHECK(a[i] == a[i - 4] && b[i] == b[i - 4]);

  // Drums, bias 0.5: the first table, kick on 0, snare on 4, from step 0.
  settings.model = 2;
  settings.bias = 0.5f;
  settings.deja_vu = 0.0f;
  MarblesGenerate(settings, 7, 16, a, b);
  CHECK(a[0] && !b[0] && b[4] && !a[4]);

  // Baking writes both tracks of the current pattern and nothing else.
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.marbles.model = 0;
  project.marbles.deja_vu = 0.0f;
  project.marbles_tracks[0] = 2;
  project.marbles_tracks[1] = 5;
  project.patterns[0].length = 16;
  engine->BakeMarbles(3);
  int hits2 = 0, hits5 = 0, other = 0;
  for (int s = 0; s < 16; ++s) {
    hits2 += project.patterns[0].tracks[2].steps[s].on;
    hits5 += project.patterns[0].tracks[5].steps[s].on;
    other += project.patterns[0].tracks[0].steps[s].on;
  }
  CHECK(hits2 + hits5 == 16 && other == 0);  // Complementary: 16 steps split.
}

// Clouds on the master: silent when dry, sane in every mode, Freeze keeps
// looping the buffer once the input has stopped.
float RenderRms(Engine* engine, size_t frames, bool* finite) {
  std::vector<float> buffer(2 * kBlockSize);
  double sum = 0.0;
  size_t count = 0;
  for (size_t done = 0; done < frames; done += kBlockSize) {
    engine->Render(buffer.data(), kBlockSize);
    for (float x : buffer) {
      if (!std::isfinite(x)) *finite = false;
      sum += x * x;
      ++count;
    }
  }
  return static_cast<float>(std::sqrt(sum / count));
}

void TestClouds() {
  // Dry/wet at zero: the effect is skipped, the mix is the plain engine's.
  {
    auto a = MakeEngine();
    auto b = MakeEngine();
    a->project().patterns[0].tracks[0].steps[0].on = true;
    b->project().patterns[0].tracks[0].steps[0].on = true;
    b->project().clouds[C_MODE] = 0.9f;  // Spectral, but dry/wet is 0.
    a->Play();
    b->Play();
    std::vector<float> x(2 * kBlockSize), y(2 * kBlockSize);
    bool same = true;
    for (int i = 0; i < 500; ++i) {
      a->Render(x.data(), kBlockSize);
      b->Render(y.data(), kBlockSize);
      same = same && x == y;
    }
    CHECK(same);
  }

  // Each mode produces finite sound on a drum loop.
  for (int mode = 0; mode < kNumCloudsModes; ++mode) {
    auto engine = MakeEngine();
    Project& project = engine->project();
    for (int s = 0; s < 16; s += 4) project.patterns[0].tracks[0].steps[s].on = true;
    project.clouds[C_MODE] = DiscreteToNormalized(mode, kNumCloudsModes);
    project.clouds[C_DRY_WET] = 1.0f;
    engine->Play();
    bool finite = true;
    RenderRms(engine.get(), 48000, &finite);
    float rms = RenderRms(engine.get(), 48000, &finite);
    CHECK(finite);
    CHECK(rms > 0.001f && rms < 1.5f);
  }

  // Freeze: after the drums stop, the frozen buffer keeps playing.
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.master[M_REVERB_LEVEL] = 0.0f;
  project.master[M_DELAY_LEVEL] = 0.0f;
  for (int s = 0; s < 16; s += 2) project.patterns[0].tracks[0].steps[s].on = true;
  project.clouds[C_MODE] = DiscreteToNormalized(2, kNumCloudsModes);  // Looping delay.
  project.clouds[C_DRY_WET] = 1.0f;
  engine->Play();
  bool finite = true;
  RenderRms(engine.get(), 48000, &finite);
  engine->clouds_freeze = true;
  engine->Stop();
  RenderRms(engine.get(), 24000, &finite);  // Let the voices die out.
  float frozen = RenderRms(engine.get(), 48000, &finite);
  CHECK(finite);
  CHECK(frozen > 0.005f);
}

// Data of the old project name: moved once, never over a newer directory.
void TestLegacyDataMigration() {
  namespace fs = std::filesystem;
  fs::path base = fs::temp_directory_path() / "drumfake_migration_test";
  fs::remove_all(base);
  fs::create_directories(base / "drumfreake" / "kits");
  std::ofstream(base / "drumfreake" / "project.json") << "{}";
  std::string project_path = (base / "drumfake" / "project.json").string();
  CHECK(MigrateLegacyDataDirectory(project_path));
  CHECK(fs::exists(project_path));
  CHECK(fs::is_directory(base / "drumfake" / "kits"));
  CHECK(!fs::exists(base / "drumfreake"));

  fs::create_directories(base / "drumfreake");
  CHECK(!MigrateLegacyDataDirectory(project_path));  // The new one exists.
  CHECK(fs::exists(base / "drumfreake"));
  fs::remove_all(base);
}

// Kit library: files, listing order, apply to the pattern or to all patterns.
void TestKitLibrary() {
  namespace fs = std::filesystem;
  fs::path dir = fs::temp_directory_path() / "drumfake_kit_test";
  fs::remove_all(dir);
  CHECK(ListKits(dir.string()).empty());  // No directory yet.

  auto engine = MakeEngine();
  Project& project = engine->project();
  std::string error;
  project.sounds[0].name = "First";
  project.sounds[0].params[P_CUTOFF] = 0.25f;
  CHECK(SaveKitFile(dir.string(), project.sounds, "", &error));
  project.sounds[0].name = "Second";
  project.sounds[0].params[P_CUTOFF] = 0.75f;
  CHECK(SaveKitFile(dir.string(), project.sounds, "", &error));
  std::vector<KitFile> kits = ListKits(dir.string());
  CHECK(kits.size() == 2);
  CHECK(kits[0].name == "Kit 1" && kits[1].name == "Kit 2");
  CHECK(kits[0].summary.rfind("First", 0) == 0);

  // Replace keeps the file and the number.
  project.sounds[0].name = "Third";
  CHECK(SaveKitFile(dir.string(), project.sounds, kits[0].path, &error));
  kits = ListKits(dir.string());
  CHECK(kits.size() == 2 && kits[0].summary.rfind("Third", 0) == 0);

  // Deleting number 1 does not renumber: the next kit is number 3.
  CHECK(DeleteKitFile(kits[0].path, &error));
  CHECK(SaveKitFile(dir.string(), project.sounds, "", &error));
  kits = ListKits(dir.string());
  CHECK(kits.size() == 2 && kits[0].name == "Kit 2" && kits[1].name == "Kit 3");

  // Loading restores names and parameters.
  TrackSound loaded[kNumTracks];
  CHECK(LoadKitFile(kits[0].path, loaded, &error));
  CHECK(loaded[0].name == "Second");
  CHECK_NEAR(loaded[0].params[P_CUTOFF], 0.75, 1e-6);
  CHECK(!LoadKitFile((dir / "missing.json").string(), loaded, &error));

  // A stray file is ignored.
  std::ofstream(dir / "notes.json") << "{}";
  std::ofstream(dir / "kit_009.json") << "not json";
  CHECK(ListKits(dir.string()).size() == 2);

  // Applying to the current pattern, then to all patterns.
  engine->ApplyKit(loaded, false);
  CHECK(project.sounds[0].name == "Second");
  CHECK(project.patterns[0].kit[0].name == "Second");
  CHECK(project.patterns[5].kit[0].name != "Second");
  engine->ApplyKit(loaded, true);
  CHECK(project.patterns[5].kit[0].name == "Second");
  engine->SelectPattern(5);  // Its kit follows.
  CHECK(project.sounds[0].name == "Second");
  fs::remove_all(dir);
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
  std::string path = (std::filesystem::temp_directory_path() / "drumfake_v1.json").string();
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

void TestDivisionsAndMicroTiming() {
  // 1/8: steps are two 16ths long (12000 samples at 120 BPM).
  auto engine = MakeEngine();
  Pattern& p = engine->project().patterns[0];
  p.division = 0;
  p.tracks[0].steps[0].on = true;
  p.tracks[0].steps[3].on = true;
  engine->Play();
  std::vector<int64_t> triggers = RenderAndCollect(engine.get(), 0, 16 * 12000 - 100);
  CHECK(triggers.size() == 2);
  if (triggers.size() == 2) {
    CHECK_NEAR(triggers[1], 3 * 12000, kBlockSize);
  }

  // 1/16T: 4000 samples per step.
  engine = MakeEngine();
  engine->project().patterns[0].division = 3;
  engine->project().patterns[0].tracks[0].steps[3].on = true;
  engine->Play();
  triggers = RenderAndCollect(engine.get(), 0, 16 * 4000 - 100);
  CHECK(triggers.size() == 1);
  if (!triggers.empty()) {
    CHECK_NEAR(triggers[0], 3 * 4000, kBlockSize);
  }

  // Micro timing: a quarter step late, half a step early.
  engine = MakeEngine();
  Pattern& q = engine->project().patterns[0];
  q.tracks[0].steps[4].on = true;
  q.tracks[0].steps[4].micro = 0.25f;
  q.tracks[1].steps[2].on = true;
  q.tracks[1].steps[2].micro = -0.5f;
  engine->Play();
  triggers = RenderAndCollect(engine.get(), 0, 8 * kSamplesPerStep);
  CHECK(triggers.size() == 1);
  if (!triggers.empty()) {
    CHECK_NEAR(triggers[0], 4.25 * kSamplesPerStep, kBlockSize);
  }
  CHECK_NEAR(engine->last_trigger_sample[1].load(), 1.5 * kSamplesPerStep, kBlockSize);

  // The metronome ticks on quarter notes whatever the division.
  engine = MakeEngine();
  engine->project().patterns[0].division = 4;  // 1/32.
  engine->metronome = true;
  engine->Play();
  std::vector<float> buffer(2 * 4 * kSamplesPerStep);
  engine->Render(buffer.data(), 4 * kSamplesPerStep);  // One quarter note.
  int clicks = 0;
  bool loud = false;
  for (size_t i = 0; i < buffer.size(); i += 2) {
    bool now = std::fabs(buffer[i]) > 0.05f;
    clicks += now && !loud;
    loud = now;
  }
  CHECK(clicks >= 1);
}

void TestSwitchAtEnd() {
  auto engine = MakeEngine();
  Project& project = engine->project();
  project.patterns[1].Clear();
  project.patterns[1].tracks[1].steps[5].on = true;
  engine->Play();
  RenderAndCollect(engine.get(), 0, 3 * kSamplesPerStep);
  // At End (default): the switch waits for the end of the pattern.
  engine->SelectPattern(1);
  CHECK(engine->current_pattern() == 0);
  // At End off: immediate, and the step count goes on (step 5 plays next).
  project.switch_at_end = false;
  engine->SelectPattern(1);
  CHECK(engine->current_pattern() == 1);
  std::vector<int64_t> t = RenderAndCollect(engine.get(), 1, 3 * kSamplesPerStep);
  CHECK(t.size() == 1);
  if (!t.empty()) {
    CHECK_NEAR(t[0], 5 * kSamplesPerStep, kBlockSize);
  }
}

void TestSoloAndRollerRecording() {
  auto engine = MakeEngine();
  Pattern& p = engine->project().patterns[0];
  for (int t = 0; t < 3; ++t) {
    p.tracks[t].steps[0].on = true;
  }
  engine->soloed[1] = true;
  engine->muted[2] = true;
  engine->Play();
  RenderAndCollect(engine.get(), 0, kSamplesPerStep);
  CHECK(engine->trigger_count[0].load() == 0);  // Not soloed.
  CHECK(engine->trigger_count[1].load() == 1);
  CHECK(engine->trigger_count[2].load() == 0);  // Muted.
  CHECK(engine->TrackAudible(1) && !engine->TrackAudible(0));

  // Repeats at 1/32 while recording: every 16th step gets two hits.
  engine = MakeEngine();
  engine->recording = true;
  engine->Play();
  engine->SetRepeat(1 << 3, 3);  // Track D, 1/32.
  RenderAndCollect(engine.get(), 3, 4 * kSamplesPerStep - 3000);
  engine->SetRepeat(0, 0);
  const Pattern& q = engine->project().patterns[0];
  for (int s = 0; s < 4; ++s) {
    CHECK(q.tracks[3].steps[s].on);
    CHECK(q.tracks[3].steps[s].ratchets == 2);
  }
  CHECK(!q.tracks[3].steps[5].on);
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
  project.chord_type = 6;
  project.patterns[3].tracks[4].steps[17].chord_size = 2;
  project.patterns[3].tracks[4].steps[17].chord[0] = 4;
  project.patterns[3].tracks[4].steps[17].chord[1] = 7;
  project.patterns[3].poly = true;
  project.patterns[3].division = 1;
  project.patterns[3].tracks[4].steps[17].micro = -0.2f;
  project.patterns[3].tracks[4].steps[17].gate = 1.5f;
  project.patterns[3].tracks[4].steps[17].slide = true;
  project.switch_at_end = false;
  project.midi_clock_sync = true;
  project.midi_channels[2] = 10;
  project.midi_trigger_note = 48;
  project.key_root = 9;
  project.key_scale = 8;
  project.marbles.model = 6;
  project.marbles.bias = 0.25f;
  project.marbles.deja_vu = 0.75f;
  project.marbles.length = 5;
  project.marbles_tracks[0] = 4;
  project.marbles_tracks[1] = 7;
  project.key_octave = -2;
  project.song_length = 3;
  project.song[0] = 2;
  project.song[1] = 2;
  project.song[2] = 5;
  project.patterns[3].tracks[4].length = 5;
  project.patterns[3].vary = 0.4f;
  project.patterns[3].vary_seed = 0xdeadbeef;

  std::string path = (std::filesystem::temp_directory_path() / "drumfake_test.json").string();
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
  CHECK(loaded->chord_type == 6);
  const Step& chord_step = loaded->patterns[3].tracks[4].steps[17];
  CHECK(chord_step.chord_size == 2 && chord_step.chord[0] == 4 && chord_step.chord[1] == 7);
  CHECK(loaded->patterns[3].tracks[4].steps[16].chord_size == 0);
  CHECK_NEAR(loaded->patterns[3].vary, 0.4, 1e-6);
  CHECK(loaded->patterns[3].vary_seed == 0xdeadbeef);
  CHECK(loaded->patterns[2].vary == 0.0f);
  CHECK(loaded->patterns[3].poly);
  CHECK(loaded->patterns[3].division == 1);
  CHECK(loaded->patterns[0].division == kDefaultDivision);
  CHECK_NEAR(loaded->patterns[3].tracks[4].steps[17].micro, -0.2, 1e-6);
  CHECK(!loaded->switch_at_end);
  CHECK(loaded->midi_clock_sync);
  CHECK(loaded->midi_channels[2] == 10);
  CHECK(loaded->midi_channels[3] == 4);
  CHECK(loaded->midi_trigger_note == 48);
  CHECK(loaded->key_root == 9);
  CHECK(loaded->key_scale == 8);
  CHECK(loaded->marbles.model == 6 && loaded->marbles.length == 5);
  CHECK_NEAR(loaded->marbles.bias, 0.25, 1e-6);
  CHECK_NEAR(loaded->marbles.deja_vu, 0.75, 1e-6);
  CHECK(loaded->marbles_tracks[0] == 4 && loaded->marbles_tracks[1] == 7);
  CHECK(loaded->key_octave == -2);
  CHECK(loaded->song_length == 3);
  CHECK(loaded->song[0] == 2 && loaded->song[1] == 2 && loaded->song[2] == 5);
  CHECK(!loaded->patterns[0].poly);
  CHECK(loaded->patterns[3].tracks[4].length == 5);
  CHECK(loaded->patterns[0].tracks[7].steps[8].has_lock(P_TIMBRE));
  CHECK_NEAR(loaded->patterns[3].tracks[4].steps[17].gate, 1.5, 1e-6);
  CHECK(loaded->patterns[3].tracks[4].steps[17].slide);
  CHECK(!loaded->patterns[3].tracks[4].steps[16].slide);
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
  TestStepGate();
  TestSnapshot();
  TestChordNotes();
  TestChordPlayback();
  TestChordRecording();
  TestStepSlide();
  TestPeaksMods();
  TestMarbles();
  TestClouds();
  TestLegacyDataMigration();
  TestKitLibrary();
  TestSong();
  TestMidiParser();
  TestMidiNotes();
  TestMidiClock();
  TestKits();
  TestClearParamLocks();
  TestCopyPages();
  TestDivisionsAndMicroTiming();
  TestSwitchAtEnd();
  TestSoloAndRollerRecording();
  TestLegacyProjectKits();
  TestLiveRecording();
  TestChoke();
  TestVary();
  TestPatternExtend();
  TestGridsPlayback();
  TestProjectRoundTrip();
  TestFullRender();
  printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
