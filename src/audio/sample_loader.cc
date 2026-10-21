#include "audio/sample_loader.h"

#include <filesystem>
#include <memory>
#include <mutex>

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

}  // namespace drumfake
