// The project's sample pool on disk: the "samples" directory beside the
// project file, the slots that name its files, and loading them.

#ifndef DRUMFAKE_CORE_SAMPLE_POOL_H_
#define DRUMFAKE_CORE_SAMPLE_POOL_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "core/params.h"
#include "core/project.h"
#include "core/sample_file.h"

namespace drumfake {

// "samples" beside the project file.
std::string SampleDirectory(const std::string& project_path);

// Copies `source` into `directory` (created if needed) and returns its name
// there. A file with the same name and the same content is reused; another
// one with the same name is kept, and the copy gets " 2", " 3"...
bool CopyIntoSampleDirectory(const std::string& source, const std::string& directory,
                             std::string* file_name, std::string* error);

// Slot holding `file`, or -1.
int FindSampleSlot(const Project& project, const std::string& file);
// First empty slot, or -1 when the pool is full.
int FirstFreeSampleSlot(const Project& project);

// The slot a sound on the Sample engine plays: its Aux parameter.
inline int SampleSlot(float aux) { return Discrete(aux, kNumSampleSlots); }
inline float SampleSlotToAux(int slot) { return DiscreteToNormalized(slot, kNumSampleSlots); }

struct SamplePoolLoad {
  std::shared_ptr<const SampleData> slots[kNumSampleSlots];
  // One line per file that could not be loaded (the slot stays empty).
  std::vector<std::string> problems;
  size_t bytes = 0;  // Memory taken by the loaded samples.
};

// Loads every file named by the pool from `directory`.
void LoadSamplePool(const Project& project, const std::string& directory, SamplePoolLoad* load);

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_SAMPLE_POOL_H_
