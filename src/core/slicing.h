// Slices of a sample: up to kMaxSlices start points, as fractions of the
// sample (the first at 0, sorted). Slice k plays from its start to the next
// one, or to the end. The Sample engine picks a slice with its Colour knob.

#ifndef DRUMFAKE_CORE_SLICING_H_
#define DRUMFAKE_CORE_SLICING_H_

#include "core/params.h"
#include "core/sample_file.h"

namespace drumfake {

const int kMaxSlices = 48;

struct SampleSlices {
  int count = 0;  // 0: not sliced.
  float starts[kMaxSlices] = {};

  void Clear() { count = 0; }
  // Where slice `k` starts and ends, as fractions of the sample.
  float start(int k) const { return starts[k]; }
  float end(int k) const { return k + 1 < count ? starts[k + 1] : 1.0f; }
};

// `count` slices of the same length.
void EqualSlices(int count, SampleSlices* slices);
// A slice at each attack (transient) of `sample`, and one at the start.
// Returns the number of slices.
int DetectSlices(const SampleData& sample, SampleSlices* slices);
// Sorts, keeps the starts in [0, 1), drops duplicates and makes the first 0.
void TidySlices(SampleSlices* slices);

// The Colour knob of the Sample engine: fully down plays the whole sample
// (-1), then one zone per slice.
inline int SliceOfColour(float colour, int count) {
  return count > 0 ? Discrete(colour, count + 1) - 1 : -1;
}
inline float ColourOfSlice(int slice, int count) {
  return DiscreteToNormalized(slice + 1, count + 1);
}

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_SLICING_H_
