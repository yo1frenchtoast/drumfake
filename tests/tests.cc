// Tests for the Drumf(re)ake core: sequencer timing, Grids port, persistence and
// a full render. Run: ./build/drumfreake_tests

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <vector>

#include "audio/engine.h"
#include "audio/wav_render.h"
#include "core/grids_gen.h"
#include "core/presets.h"
#include "core/project_io.h"
#include "grids/resources.h"

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
  project.chords[2] = 0x42;
  project.patterns[3].poly = true;
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
  CHECK(loaded->sounds[0].name == project.sounds[0].name);
  CHECK(loaded->chords[2] == 0x42);
  CHECK(loaded->patterns[3].poly);
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
  TestGrids();
  TestStepTiming();
  TestSwing();
  TestRatchetsAndProbability();
  TestTrackLengthAndPatternChain();
  TestPolyrhythm();
  TestLockFeedback();
  TestSong();
  TestLiveRecording();
  TestChoke();
  TestGridsPlayback();
  TestProjectRoundTrip();
  TestFullRender();
  printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
