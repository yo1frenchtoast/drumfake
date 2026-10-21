// Reading a sample between its frames, shared by the sample and grain players.

#ifndef DRUMFAKE_AUDIO_SAMPLE_READ_H_
#define DRUMFAKE_AUDIO_SAMPLE_READ_H_

#include <algorithm>
#include <cstdint>

namespace drumfake {

// 4-point Hermite interpolation of `d` (n frames, n >= 1) at position `p`,
// clamped to the first and last frames. Returns [-1, 1].
inline float ReadSample(const int16_t* d, int n, double p) {
  if (p <= 0.0) {
    return d[0] / 32768.0f;
  }
  if (p >= n - 1) {
    return d[n - 1] / 32768.0f;
  }
  int i = static_cast<int>(p);
  float t = static_cast<float>(p - i);
  float xm1 = d[std::max(i - 1, 0)];
  float x0 = d[i];
  float x1 = d[std::min(i + 1, n - 1)];
  float x2 = d[std::min(i + 2, n - 1)];
  float c = (x1 - xm1) * 0.5f;
  float v = x0 - x1;
  float w = c + v;
  float a = w + v + (x2 - x0) * 0.5f;
  float b = w + a;
  return ((((a * t) - b) * t + c) * t + x0) / 32768.0f;
}

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_SAMPLE_READ_H_
