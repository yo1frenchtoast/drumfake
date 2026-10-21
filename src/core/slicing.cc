#include "core/slicing.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace drumfake {

void EqualSlices(int count, SampleSlices* slices) {
  count = std::clamp(count, 1, kMaxSlices);
  for (int k = 0; k < count; ++k) {
    slices->starts[k] = static_cast<float>(k) / count;
  }
  slices->count = count;
}

void TidySlices(SampleSlices* slices) {
  std::vector<float> starts;
  for (int k = 0; k < std::clamp(slices->count, 0, kMaxSlices); ++k) {
    float s = slices->starts[k];
    if (std::isfinite(s) && s >= 0.0f && s < 1.0f) {
      starts.push_back(s);
    }
  }
  if (starts.empty()) {
    slices->count = 0;
    return;
  }
  starts.push_back(0.0f);
  std::sort(starts.begin(), starts.end());
  starts.erase(std::unique(starts.begin(), starts.end(),
                           [](float a, float b) { return b - a < 1e-6f; }),
               starts.end());
  slices->count = std::min(static_cast<int>(starts.size()), kMaxSlices);
  std::copy(starts.begin(), starts.begin() + slices->count, slices->starts);
}

int DetectSlices(const SampleData& sample, SampleSlices* slices) {
  // The loudest point of each 5 ms hop; an attack is a hop much louder than
  // the few before it, loud enough against the whole sample, at least 50 ms
  // after the previous attack.
  const size_t kHop = static_cast<size_t>(kSampleFileRate / 200);
  const size_t hops = sample.frames.size() / kHop;
  std::vector<float> level(hops, 0.0f);
  float loudest = 0.0f;
  for (size_t h = 0; h < hops; ++h) {
    int peak = 0;
    for (size_t i = h * kHop; i < (h + 1) * kHop; ++i) {
      peak = std::max(peak, std::abs(static_cast<int>(sample.frames[i])));
    }
    level[h] = peak / 32768.0f;
    loudest = std::max(loudest, level[h]);
  }
  slices->count = 1;
  slices->starts[0] = 0.0f;
  if (loudest <= 0.0f) {
    return 1;
  }
  const float threshold = loudest * 0.1f;  // -20 dB.
  const size_t kHistory = 4;
  const size_t kMinGap = 10;               // 50 ms.
  size_t last = 0;
  bool first_attack = true;
  for (size_t h = 1; h < hops && slices->count < kMaxSlices; ++h) {
    float before = 0.0f;
    size_t from = h >= kHistory ? h - kHistory : 0;
    for (size_t k = from; k < h; ++k) {
      before = std::max(before, level[k]);
    }
    if (level[h] < threshold || level[h] < 2.0f * before + 1e-4f) {
      continue;
    }
    if (!first_attack && h - last < kMinGap) {
      continue;
    }
    // The attack starts in the previous hop at the latest.
    size_t frame = (h - 1) * kHop;
    float start = static_cast<float>(frame) / static_cast<float>(sample.frames.size());
    if (first_attack && h < kMinGap) {
      // An attack right at the start is the first slice.
      slices->starts[0] = 0.0f;
    } else {
      slices->starts[slices->count++] = start;
    }
    first_attack = false;
    last = h;
  }
  return slices->count;
}

}  // namespace drumfake
