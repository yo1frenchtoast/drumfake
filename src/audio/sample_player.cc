#include "audio/sample_player.h"

#include <algorithm>
#include <cmath>

#include "core/params.h"

namespace drumfake {

namespace {

const int kFadeIn = 24;               // 0.5 ms declick when Start is not the beginning.
const double kEndFade = 96.0;         // 2 ms fade before the end of a one-shot.
const double kMaxCrossfade = 480.0;   // 10 ms across the loop point.
const double kMinLoop = 64.0;         // Shortest loop, in frames.

// 4-point Hermite interpolation at position `p` (0 to n - 1).
inline float Read(const int16_t* d, int n, double p) {
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

}  // namespace

void SamplePlayer::Init() {
  pending_ = false;
  playing_ = false;
  played_ = 0.0;
  envelope_ = 0.0f;
  fade_in_ = 0;
}

void SamplePlayer::Start() {
  pending_ = true;
}

void SamplePlayer::Render(const SampleData* sample, const float* params, float note, bool gate,
                          float* out) {
  const int n = sample ? static_cast<int>(sample->frames.size()) : 0;
  if (pending_) {
    pending_ = false;
    playing_ = true;
    played_ = 0.0;
    envelope_ = 1.0f;
    fade_in_ = params[P_HARMONICS] > 0.001f ? kFadeIn : 0;
  }
  if (!playing_ || n < 4) {
    playing_ = false;  // No sample (an empty slot, or it was removed): nothing to resume.
    std::fill(out, out + kBlockSize, 0.0f);
    return;
  }

  const double last = n - 1;
  const double start = params[P_HARMONICS] * last;
  const double span = (params[P_TIMBRE] * 2.0 - 1.0) * n;
  const double end = std::clamp(start + span, 0.0, last);
  const double direction = end >= start ? 1.0 : -1.0;
  const double length = std::fabs(end - start);
  if (length < 1.0) {
    playing_ = false;
    std::fill(out, out + kBlockSize, 0.0f);
    return;
  }
  const bool looping = params[P_MORPH] > 0.02f && length > 2.0 * kMinLoop;
  const double loop_from = looping ? std::min(length * params[P_MORPH], length - kMinLoop) : length;
  const double loop_length = length - loop_from;
  const double crossfade = looping ? std::min({ kMaxCrossfade, loop_length * 0.5, loop_from }) : 0.0;
  const double rate = std::clamp(std::pow(2.0, (note - kSampleRootNote) / 12.0), 1.0 / 64.0, 16.0);
  // Fully up, Decay holds the sample to its end.
  const float release = params[P_DECAY] >= 0.98f
      ? 1.0f
      : expf(-1.0f / (FilterDecayToSeconds(params[P_DECAY]) * kSampleRate));
  const int16_t* d = sample->frames.data();

  for (size_t i = 0; i < kBlockSize; ++i) {
    if (!playing_) {
      out[i] = 0.0f;
      continue;
    }
    if (played_ >= length) {
      if (!looping) {
        playing_ = false;
        out[i] = 0.0f;
        continue;
      }
      played_ = loop_from + std::fmod(played_ - loop_from, loop_length);
    }
    float s = Read(d, n, start + direction * played_);
    if (looping && played_ > length - crossfade) {
      float w = static_cast<float>((played_ - (length - crossfade)) / crossfade);
      s += (Read(d, n, start + direction * (played_ - loop_length)) - s) * w;
    } else if (!looping && played_ > length - kEndFade) {
      s *= static_cast<float>((length - played_) / kEndFade);
    }
    if (fade_in_ > 0) {
      s *= static_cast<float>(kFadeIn - fade_in_) / kFadeIn;
      --fade_in_;
    }
    if (!gate) {
      envelope_ *= release;
    }
    out[i] = s * envelope_;
    played_ += rate;
    if (envelope_ < 1e-4f) {
      playing_ = false;
    }
  }
}

}  // namespace drumfake
