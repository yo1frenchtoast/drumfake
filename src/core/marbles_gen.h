// Random rhythm generator built on the Mutable Instruments Marbles "t"
// section (marbles::TGenerator), turned into step patterns.
//
// Kept free of Marbles headers: marbles/resources.h defines LUT_* macros.

#ifndef DRUMFAKE_CORE_MARBLES_GEN_H_
#define DRUMFAKE_CORE_MARBLES_GEN_H_

#include <cstdint>

namespace drumfake {

const int kNumMarblesModels = 7;
extern const char* const kMarblesModelNames[kNumMarblesModels];

// Longest "deja vu" loop, in steps.
const int kMaxDejaVuLength = 16;

struct MarblesSettings {
  int model = 0;         // Index in kMarblesModelNames.
  float bias = 0.5f;     // 0 to 1: meaning depends on the model (density, pattern choice...).
  float deja_vu = 0.5f;  // 0 = always new, 1 = locked loop of `length` steps.
  int length = 8;        // Deja vu loop length, 1 to kMaxDejaVuLength steps.
};

// Generates `num_steps` steps for the two "t" outputs (t1 and t3 of the
// module). The same seed gives the same result.
void MarblesGenerate(const MarblesSettings& settings, uint32_t seed,
                     int num_steps, bool* t1, bool* t3);

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_MARBLES_GEN_H_
