#include "audio/sample_loader.h"

#include <filesystem>
#include <memory>
#include <mutex>

#include "audio/wav_render.h"
#include "core/sample_pool.h"

namespace drumfake {

void ApplySampleDefaults(TrackSound* sound, int slot, const std::string& file) {
  sound->name = std::filesystem::path(file).stem().string().substr(0, 16);
  sound->params[P_ENGINE] = DiscreteToNormalized(kSampleEngine, kNumEngines);
  sound->params[P_AUX] = SampleSlotToAux(slot);
  sound->params[P_HARMONICS] = 0.0f;  // Start: the beginning.
  sound->params[P_TIMBRE] = 1.0f;     // Length: all of it, forwards.
  sound->params[P_MORPH] = 0.0f;      // No loop.
  sound->params[P_DECAY] = 1.0f;      // Plays to the end.
  sound->params[P_COLOUR] = 0.0f;     // The whole sample, not a slice.
  sound->params[P_PITCH] = NoteToNormalizedPitch(kSampleRootNote);
}

void LoadProjectSamples(Engine* engine, const std::string& project_path,
                        std::vector<std::string>* problems) {
  auto project = std::make_unique<Project>();
  {
    std::lock_guard<std::mutex> lock(engine->mutex());
    std::copy(engine->project().samples, engine->project().samples + kNumSampleSlots,
              project->samples);
  }
  SamplePoolLoad load;
  LoadSamplePool(*project, SampleDirectory(project_path), &load);
  {
    std::lock_guard<std::mutex> lock(engine->mutex());
    engine->SwapSamples(load.slots);
  }
  // `load` now holds the previous samples: released here, outside the lock.
  problems->insert(problems->end(), load.problems.begin(), load.problems.end());
}

bool ImportSample(Engine* engine, const std::string& project_path, const std::string& source,
                  int track, std::string* message) {
  auto sample = std::make_shared<SampleData>();
  std::string error;
  if (!LoadWav(source, sample.get(), &error)) {
    *message = error;
    return false;
  }
  std::string file;
  if (!CopyIntoSampleDirectory(source, SampleDirectory(project_path), &file, &error)) {
    *message = error;
    return false;
  }
  std::shared_ptr<const SampleData> previous = sample;
  std::string name = std::filesystem::path(file).stem().string();
  int slot;
  {
    std::lock_guard<std::mutex> lock(engine->mutex());
    Project& project = engine->project();
    slot = FindSampleSlot(project, file);
    if (slot < 0) {
      slot = FirstFreeSampleSlot(project);
      if (slot >= 0) {
        project.slices[slot].Clear();  // A new sample: no slices yet.
      }
    }
    if (slot < 0) {
      *message = "The sample pool is full (" + std::to_string(kNumSampleSlots) + " slots)";
      return false;
    }
    project.samples[slot] = file;
    engine->SwapSample(slot, &previous);
    ApplySampleDefaults(&project.sounds[track], slot, file);
  }
  char text[160];
  snprintf(text, sizeof(text), "%s on track %c (slot %d, %.1fs%s)", name.c_str(), 'A' + track,
           slot + 1, sample->seconds(), sample->truncated ? ", cut" : "");
  *message = text;
  return true;
}

bool AddRecordedSample(Engine* engine, const std::string& project_path,
                       std::vector<int16_t> frames, int* slot, std::string* message) {
  namespace fs = std::filesystem;
  if (frames.empty()) {
    *message = "Nothing was recorded";
    return false;
  }
  const fs::path directory = SampleDirectory(project_path);
  std::error_code ec;
  fs::create_directories(directory, ec);
  std::string name;
  for (int n = 1; n < 1000 && name.empty(); ++n) {
    char candidate[32];
    snprintf(candidate, sizeof(candidate), "Resample %03d.wav", n);
    if (!fs::exists(directory / candidate, ec)) {
      name = candidate;
    }
  }
  std::vector<float> audio(frames.size());
  for (size_t i = 0; i < frames.size(); ++i) {
    audio[i] = frames[i] / 32767.0f;
  }
  if (name.empty() || !WriteWav((directory / name).string(), audio, 1, kSampleFileRate)) {
    *message = "Cannot write the recording in " + directory.string();
    return false;
  }
  auto sample = std::make_shared<SampleData>();
  sample->frames = std::move(frames);
  sample->source_rate = kSampleFileRate;
  sample->source_channels = 1;
  sample->source_bits = 16;
  const float seconds = sample->seconds();
  std::shared_ptr<const SampleData> previous = std::move(sample);
  {
    std::lock_guard<std::mutex> lock(engine->mutex());
    Project& project = engine->project();
    *slot = FirstFreeSampleSlot(project);
    if (*slot < 0) {
      *message = name + " saved, but the sample pool is full";
      return false;
    }
    project.samples[*slot] = name;
    project.slices[*slot].Clear();
    engine->SwapSample(*slot, &previous);
  }
  char text[128];
  snprintf(text, sizeof(text), "%s: %.2fs in slot %d (Browse > Samples)",
           fs::path(name).stem().string().c_str(), seconds, *slot + 1);
  *message = text;
  return true;
}

}  // namespace drumfake
