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
// `sensitivity` (0 to 1) is the share of the attacks kept, the strongest
// first: 1 keeps them all (ghost notes and cymbals too), lower keeps the main
// hits. Returns the number of slices.
int DetectSlices(const SampleData& sample, SampleSlices* slices, float sensitivity = 1.0f);
// Sorts, keeps the starts in [0, 1), drops duplicates and makes the first 0.
void TidySlices(SampleSlices* slices);

// Manual editing. Positions are fractions of the sample; slice 0 always starts
// at 0, and two starts are at least kMinSliceGap apart.
const float kMinSliceGap = 0.001f;
// The slice playing `position`.
int SliceAt(const SampleSlices& slices, float position);
// Adds a slice starting at `position`; returns its index, or -1 (full, or too
// close to another start). An unsliced sample gets its first slice at 0 too.
int AddSlice(SampleSlices* slices, float position);
// Removes slice `k` (not the first): the previous one runs on to the next.
bool RemoveSlice(SampleSlices* slices, int k);
// Moves the start of slice `k` (not the first), kept between its neighbours.
// Returns where it went.
float MoveSlice(SampleSlices* slices, int k, float position);
// The zero crossing of `sample` nearest to `frame`, within `radius` frames (or
// `frame` when there is none): a slice starting there does not click.
int NearestZeroCrossing(const SampleData& sample, int frame, int radius);

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
