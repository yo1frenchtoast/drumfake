// Bringing samples into the engine: the project's pool when a project is
// loaded, and a file imported onto a track. Both read and decode files without
// the engine lock, then swap the samples in under it, and release the previous
// ones after: call them without holding the lock.

#ifndef DRUMFAKE_AUDIO_SAMPLE_LOADER_H_
#define DRUMFAKE_AUDIO_SAMPLE_LOADER_H_

#include <string>
#include <vector>

#include "audio/engine.h"

namespace drumfake {

// Sets `sound` to play `slot` (holding `file`): the Sample engine, the whole
// sample once, at its own pitch, named after the file.
void ApplySampleDefaults(TrackSound* sound, int slot, const std::string& file);

// Loads every slot of the project's pool from the samples directory beside
// `project_path`. `problems` gets one line per file that could not be loaded.
void LoadProjectSamples(Engine* engine, const std::string& project_path,
                        std::vector<std::string>* problems);

// Copies `source` (a WAV file) into the samples directory, puts it in a slot of
// the pool (the one already holding it, or the first free one) and sets
// `track` to play it: the Sample engine, the whole sample once, at its own
// pitch. `message` says what happened (or what went wrong).
bool ImportSample(Engine* engine, const std::string& project_path, const std::string& source,
                  int track, std::string* message);

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_SAMPLE_LOADER_H_
