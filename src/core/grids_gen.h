// Port of the Mutable Instruments Grids "drums" pattern generator.
//
// The pattern tables come unmodified from grids/resources.cc; the map
// interpolation and the density/perturbation logic reproduce
// grids::PatternGenerator::EvaluateDrums() without the AVR dependencies.

#ifndef DRUMFAKE_CORE_GRIDS_GEN_H_
#define DRUMFAKE_CORE_GRIDS_GEN_H_

#include <cstdint>

namespace drumfake {

const int kGridsParts = 3;         // BD, SD, HH.
const int kGridsStepsPerBar = 32;  // Grids works with 32 steps per pattern.

class GridsGenerator {
 public:
  void Init(uint32_t seed);

  // Raw level (0-255) of `part` at `step` for map position (x, y).
  static uint8_t ReadDrumMap(int step, int part, uint8_t x, uint8_t y);

  // Evaluates one Grids step (0-31). Returns the triggered parts as a bit
  // mask; accented parts are stored in *accents.
  uint8_t Evaluate(int step, uint8_t x, uint8_t y, const uint8_t* density,
                   uint8_t randomness, uint8_t* accents);

  // Evaluates the two Grids steps that fall on one 16th note, as the module
  // does when clocked at 4 PPQN.
  uint8_t EvaluateSixteenth(int sixteenth, uint8_t x, uint8_t y,
                            const uint8_t* density, uint8_t randomness,
                            uint8_t* accents);

 private:
  uint8_t RandomByte();

  uint32_t rng_state_;
  uint8_t perturbation_[kGridsParts];
};

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_GRIDS_GEN_H_
