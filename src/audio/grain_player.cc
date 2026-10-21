#include "audio/grain_player.h"

#include <algorithm>
#include <cmath>

#include "audio/sample_read.h"
#include "core/params.h"

namespace drumfake {

namespace {

const float kTwoPi = 6.28318530718f;

}  // namespace

void GrainPlayer::Init(uint32_t seed) {
  for (Grain& g : grains_) {
    g.alive = false;
  }
  live_ = 0;
  rng_ = seed ? seed : 1;
  pending_ = false;
  spawning_ = false;
  scan_ = 0.0;
  countdown_ = 0.0;
  envelope_ = 0.0f;
  burst_ = 0.0f;
}

void GrainPlayer::Start() {
  pending_ = true;
}

float GrainPlayer::Random() {
  // xorshift32.
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return static_cast<float>(rng_ >> 8) / 16777216.0f;
}

void GrainPlayer::Spawn(int engine, int n, double position, double rate, float size,
                        float chaos, float gain) {
  for (Grain& g : grains_) {
    if (g.alive) {
      continue;
    }
    // Chaos: a little later or earlier in the sample, longer or shorter, detuned.
    g.length = std::max(32.0f, size * kSampleRate * (1.0f + chaos * (Random() - 0.5f)));
    const float spread = engine == kSprayEngine ? 0.2f * static_cast<float>(n)
                                                : g.length * static_cast<float>(rate);
    double start = position + chaos * (Random() - 0.5f) * spread;
    float detune = chaos * (Random() * 2.0f - 1.0f) * (engine == kBurstEngine ? 1.0f : 2.0f);
    g.rate = rate * std::pow(2.0, detune / 12.0);
    // Keep the whole grain inside the sample.
    double reach = g.length * g.rate;
    start = std::clamp(start, 0.0, std::max(0.0, static_cast<double>(n - 1) - reach));
    g.position = start;
    g.age = 0.0f;
    g.gain = gain;
    g.alive = true;
    ++live_;
    return;
  }
  // Every grain is busy: this one is skipped.
}

void GrainPlayer::Render(const SampleData* sample, int engine, const float* params, float note,
                         bool gate, float* out) {
  const int n = sample ? static_cast<int>(sample->frames.size()) : 0;
  if (n < 64) {
    // No sample (or a tiny one): nothing to play or resume.
    for (Grain& g : grains_) {
      g.alive = false;
    }
    live_ = 0;
    pending_ = spawning_ = false;
    std::fill(out, out + kBlockSize, 0.0f);
    return;
  }
  if (pending_) {
    pending_ = false;
    spawning_ = true;
    scan_ = 0.0;
    countdown_ = 0.0;
    envelope_ = 1.0f;
    burst_ = 1.0f;
  }

  const bool burst = engine == kBurstEngine;
  const float density = 2.0f * powf(100.0f, params[P_TIMBRE]);
  const float chaos = burst ? params[P_COLOUR] : params[P_MORPH];
  const float size = burst ? 0.005f * powf(40.0f, params[P_MORPH])
                           : 0.01f * powf(50.0f, params[P_COLOUR]);
  const double rate = std::clamp(std::pow(2.0, (note - 60.0f) / 12.0), 1.0 / 64.0, 16.0);
  const float harmonics = params[P_HARMONICS];
  const double scan_speed = harmonics < 0.5f ? harmonics * 2.0f : 1.0f + (harmonics - 0.5f) * 6.0f;
  const double position = harmonics * (n - 1);
  const float release = expf(-1.0f / (GrainReleaseToSeconds(params[P_DECAY]) * kSampleRate));
  // Burst: the grains get softer over a time set by Shape (20 ms to 1 s).
  const float burst_decay =
      expf(-1.0f / (0.02f * powf(50.0f, params[P_MORPH]) * kSampleRate));
  // Overlapping grains add up: keep the level steady as density and size grow.
  // The Hann window of Scrub and Spray averages 0.61 of the sample's level: made up.
  const float overlap = std::min(static_cast<float>(kMaxGrains), density * size);
  const float normalize = (burst ? 1.0f : 1.5f) / std::sqrt(std::max(1.0f, overlap));
  const int16_t* d = sample->frames.data();

  for (size_t i = 0; i < kBlockSize; ++i) {
    if (spawning_) {
      countdown_ -= 1.0;
      if (countdown_ <= 0.0) {
        double from = engine == kScrubEngine ? scan_ : position;
        Spawn(engine, n, from, rate, size, chaos, burst ? burst_ : 1.0f);
        countdown_ += std::max(1.0, kSampleRate / density * (1.0 + chaos * (Random() * 1.6 - 0.8)));
      }
      if (engine == kScrubEngine) {
        scan_ = std::fmod(scan_ + scan_speed, static_cast<double>(n));
      }
      if (burst) {
        burst_ *= burst_decay;
        spawning_ = burst_ > 1e-3f;
      }
    }
    if (!gate) {
      envelope_ *= release;
      if (envelope_ < 1e-4f) {
        spawning_ = false;
      }
    }

    float sum = 0.0f;
    for (Grain& g : grains_) {
      if (!g.alive) {
        continue;
      }
      float phase = g.age / g.length;
      float window;
      if (burst) {
        // Percussive: a quick attack, then a curved decay.
        window = phase < 0.05f ? phase / 0.05f : (1.0f - phase) * (1.0f - phase) / 0.9025f;
      } else {
        window = 0.5f - 0.5f * cosf(kTwoPi * phase);
      }
      sum += ReadSample(d, n, g.position + g.age * g.rate) * window * g.gain;
      g.age += 1.0f;
      if (g.age >= g.length) {
        g.alive = false;
        --live_;
      }
    }
    out[i] = sum * normalize * envelope_;
  }
}

}  // namespace drumfake
