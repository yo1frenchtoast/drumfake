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

namespace {

// In-place radix-2 FFT of `re` + i `im` (size a power of two).
void Fft(std::vector<float>* re, std::vector<float>* im) {
  const size_t n = re->size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      std::swap((*re)[i], (*re)[j]);
      std::swap((*im)[i], (*im)[j]);
    }
  }
  for (size_t length = 2; length <= n; length <<= 1) {
    const double angle = -2.0 * 3.14159265358979323846 / static_cast<double>(length);
    for (size_t i = 0; i < n; i += length) {
      for (size_t k = 0; k < length / 2; ++k) {
        float wr = static_cast<float>(std::cos(angle * static_cast<double>(k)));
        float wi = static_cast<float>(std::sin(angle * static_cast<double>(k)));
        size_t a = i + k;
        size_t b = a + length / 2;
        float br = (*re)[b] * wr - (*im)[b] * wi;
        float bi = (*re)[b] * wi + (*im)[b] * wr;
        (*re)[b] = (*re)[a] - br;
        (*im)[b] = (*im)[a] - bi;
        (*re)[a] += br;
        (*im)[a] += bi;
      }
    }
  }
}

}  // namespace

int DetectSlices(const SampleData& sample, SampleSlices* slices, float sensitivity) {
  // Spectral flux: how much new energy each 256-frame hop brings, summed over
  // the spectrum (log magnitudes). A hit stands out in its own band even over
  // a sustained cymbal, where the overall level hardly moves. An attack is a
  // local peak of the flux above a running median, at least 50 ms after the
  // previous one (the stronger of two closer ones). Then the strongest share
  // set by `sensitivity` is kept, at most kMaxSlices.
  const size_t kSize = 1024;
  const size_t kHop = 256;
  const size_t n = sample.frames.size();
  slices->count = 1;
  slices->starts[0] = 0.0f;
  if (n < kSize * 2) {
    return 1;
  }
  std::vector<float> window(kSize);
  for (size_t i = 0; i < kSize; ++i) {
    window[i] = 0.5f - 0.5f * cosf(2.0f * 3.14159265f * static_cast<float>(i) / kSize);
  }
  const size_t frames = (n - kSize) / kHop;
  std::vector<float> previous(kSize / 2 + 1, 0.0f);
  std::vector<float> flux(frames, 0.0f);
  std::vector<float> re(kSize);
  std::vector<float> im(kSize);
  for (size_t f = 0; f < frames; ++f) {
    for (size_t i = 0; i < kSize; ++i) {
      re[i] = sample.frames[f * kHop + i] / 32768.0f * window[i];
      im[i] = 0.0f;
    }
    Fft(&re, &im);
    float sum = 0.0f;
    for (size_t k = 0; k <= kSize / 2; ++k) {
      float magnitude = std::log1p(100.0f * std::sqrt(re[k] * re[k] + im[k] * im[k]));
      if (f > 0) {
        sum += std::max(0.0f, magnitude - previous[k]);
      }
      previous[k] = magnitude;
    }
    flux[f] = sum;
  }
  const float strongest = *std::max_element(flux.begin(), flux.end());
  if (strongest <= 0.0f) {
    return 1;
  }
  const int kNeighbours = 8;        // The median of the flux around a frame...
  const float kAboveMedian = 1.5f;  // ...times this, plus a floor, is the threshold.
  const size_t kMinGap = static_cast<size_t>(0.05 * kSampleFileRate);
  const size_t kEarly = kSampleFileRate / 500;   // Start 2 ms before the attack.
  // Below this, the flux is the rounding noise of a steady sound (a real hit
  // brings hundreds to thousands).
  const float kFloor = 20.0f;
  std::vector<float> around;
  std::vector<std::pair<float, size_t>> attacks;  // Strength, start frame.
  for (size_t f = 1; f + 1 < frames; ++f) {
    if (flux[f] < flux[f - 1] || flux[f] < flux[f + 1]) {
      continue;
    }
    size_t lo = f >= static_cast<size_t>(kNeighbours) ? f - kNeighbours : 0;
    size_t hi = std::min(frames, f + kNeighbours + 1);
    around.assign(flux.begin() + static_cast<std::ptrdiff_t>(lo),
                  flux.begin() + static_cast<std::ptrdiff_t>(hi));
    std::nth_element(around.begin(), around.begin() + static_cast<std::ptrdiff_t>(around.size() / 2),
                     around.end());
    float threshold = std::max(kFloor, around[around.size() / 2] * kAboveMedian + 0.02f * strongest);
    if (flux[f] <= threshold) {
      continue;
    }
    // An attack enters the window from its end: the flux peaks as it crosses
    // the steepest part of the Hann window, three quarters into it.
    size_t onset = f * kHop + 3 * kSize / 4 - kHop / 2;
    onset = onset > kEarly ? onset - kEarly : 0;
    const float strength = flux[f] - threshold;
    if (!attacks.empty() && onset - attacks.back().second < kMinGap) {
      if (strength > attacks.back().first) {
        attacks.back() = { strength, onset };  // The stronger of two close attacks.
      }
      continue;
    }
    attacks.push_back({ strength, onset });
  }
  if (!attacks.empty() && attacks.front().second < kMinGap) {
    attacks.erase(attacks.begin());  // An attack at the very start is the first slice.
  }
  const size_t keep = std::min(
      static_cast<size_t>(kMaxSlices - 1),
      static_cast<size_t>(std::ceil(std::clamp(sensitivity, 0.0f, 1.0f) * attacks.size())));
  if (attacks.size() > keep) {
    std::sort(attacks.begin(), attacks.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    attacks.resize(keep);
    std::sort(attacks.begin(), attacks.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
  }
  for (const auto& attack : attacks) {
    slices->starts[slices->count++] = static_cast<float>(attack.second) / static_cast<float>(n);
  }
  return slices->count;
}

}  // namespace drumfake
