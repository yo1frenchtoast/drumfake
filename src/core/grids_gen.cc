#include "core/grids_gen.h"

#include "grids/resources.h"

namespace drumfreake {

using namespace grids;

namespace {

// Same layout as the drum_map table of grids/pattern_generator.cc.
const uint8_t* const kDrumMap[5][5] = {
  { node_10, node_8, node_0, node_9, node_11 },
  { node_15, node_7, node_13, node_12, node_6 },
  { node_18, node_14, node_4, node_5, node_3 },
  { node_23, node_16, node_21, node_1, node_2 },
  { node_24, node_19, node_17, node_20, node_22 },
};

// avrlib's U8Mix: crossfade between a and b.
inline uint8_t U8Mix(uint8_t a, uint8_t b, uint8_t balance) {
  return static_cast<uint8_t>(
      (a * (255 - balance) + b * balance) >> 8);
}

}  // namespace

void GridsGenerator::Init(uint32_t seed) {
  rng_state_ = seed ? seed : 0x21;
  for (int i = 0; i < kGridsParts; ++i) {
    perturbation_[i] = 0;
  }
}

uint8_t GridsGenerator::RandomByte() {
  // xorshift32, in place of avrlib's LFSR.
  rng_state_ ^= rng_state_ << 13;
  rng_state_ ^= rng_state_ >> 17;
  rng_state_ ^= rng_state_ << 5;
  return static_cast<uint8_t>(rng_state_ >> 24);
}

/* static */
uint8_t GridsGenerator::ReadDrumMap(int step, int part, uint8_t x, uint8_t y) {
  uint8_t i = x >> 6;
  uint8_t j = y >> 6;
  const uint8_t* a_map = kDrumMap[i][j];
  const uint8_t* b_map = kDrumMap[i + 1][j];
  const uint8_t* c_map = kDrumMap[i][j + 1];
  const uint8_t* d_map = kDrumMap[i + 1][j + 1];
  int offset = part * kGridsStepsPerBar + step;
  uint8_t a = a_map[offset];
  uint8_t b = b_map[offset];
  uint8_t c = c_map[offset];
  uint8_t d = d_map[offset];
  uint8_t fx = static_cast<uint8_t>(x << 2);
  uint8_t fy = static_cast<uint8_t>(y << 2);
  return U8Mix(U8Mix(a, b, fx), U8Mix(c, d, fx), fy);
}

uint8_t GridsGenerator::Evaluate(
    int step, uint8_t x, uint8_t y, const uint8_t* density,
    uint8_t randomness, uint8_t* accents) {
  // At the beginning of a pattern, decide on perturbation levels.
  if (step == 0) {
    for (int i = 0; i < kGridsParts; ++i) {
      perturbation_[i] = static_cast<uint8_t>(
          (RandomByte() * (randomness >> 2)) >> 8);
    }
  }
  uint8_t state = 0;
  uint8_t accent_bits = 0;
  for (int i = 0; i < kGridsParts; ++i) {
    int level = ReadDrumMap(step, i, x, y);
    if (level < 255 - perturbation_[i]) {
      level += perturbation_[i];
    } else {
      level = 255;
    }
    uint8_t threshold = static_cast<uint8_t>(~density[i]);
    if (level > threshold) {
      if (level > 192) {
        accent_bits |= 1 << i;
      }
      state |= 1 << i;
    }
  }
  *accents = accent_bits;
  return state;
}

uint8_t GridsGenerator::EvaluateSixteenth(
    int sixteenth, uint8_t x, uint8_t y, const uint8_t* density,
    uint8_t randomness, uint8_t* accents) {
  int step = (sixteenth * 2) % kGridsStepsPerBar;
  uint8_t accents_1 = 0;
  uint8_t accents_2 = 0;
  uint8_t state = Evaluate(step, x, y, density, randomness, &accents_1);
  state |= Evaluate(step + 1, x, y, density, randomness, &accents_2);
  *accents = accents_1 | accents_2;
  return state;
}

}  // namespace drumfreake
