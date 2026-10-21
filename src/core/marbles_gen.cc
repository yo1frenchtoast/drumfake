#include "core/marbles_gen.h"

#include <algorithm>
#include <memory>

#include "marbles/random/random_generator.h"
#include "marbles/random/random_stream.h"
#include "marbles/random/t_generator.h"

namespace drumfake {

// In the order of marbles::TGeneratorModel.
const char* const kMarblesModelNames[kNumMarblesModels] = {
  "Complementary", "Clusters", "Drums", "Independent", "Divider", "Three states", "Markov"
};

namespace {

// The generator is clocked internally at one tick per kSamplesPerStep samples.
// TGenerator::Init() takes a sample rate from which it derives the internal
// frequency (2 * rate_hz / sr at its default range): 2 / sr = 1 / 1000.
const int kSamplesPerStep = 1000;
const float kClockRate = 2.0f * kSamplesPerStep;
const size_t kBlock = 64;

}  // namespace

void MarblesGenerate(const MarblesSettings& settings, uint32_t seed,
                     int num_steps, bool* t1, bool* t3) {
  std::fill(t1, t1 + num_steps, false);
  std::fill(t3, t3 + num_steps, false);

  auto random_generator = std::make_unique<marbles::RandomGenerator>();
  auto random_stream = std::make_unique<marbles::RandomStream>();
  auto generator = std::make_unique<marbles::TGenerator>();
  random_generator->Init(seed);
  random_stream->Init(random_generator.get());
  generator->Init(random_stream.get(), kClockRate);

  generator->set_model(static_cast<marbles::TGeneratorModel>(
      std::max(0, std::min(kNumMarblesModels - 1, settings.model))));
  generator->set_range(marbles::T_GENERATOR_RANGE_1X);
  generator->set_rate(0.0f);
  generator->set_bias(settings.bias);
  generator->set_jitter(0.0f);
  // On the module, deja vu is random at minimum, a locked loop at noon, and
  // mutates the loop beyond. Only the first half is useful here.
  generator->set_deja_vu(0.5f * std::max(0.0f, std::min(1.0f, settings.deja_vu)));
  generator->set_length(std::max(1, std::min(kMaxDejaVuLength, settings.length)));
  generator->set_pulse_width_mean(0.5f);
  generator->set_pulse_width_std(0.0f);

  float master[kBlock], external[kBlock], slave[2][kBlock];
  bool gates[2 * kBlock];
  marbles::Ramps ramps;
  ramps.master = master;
  ramps.external = external;
  ramps.slave[0] = slave[0];
  ramps.slave[1] = slave[1];

  // A reset at the start schedules the first pulses, which the module outputs
  // at the next tick: a gate that rises on tick n belongs to step n - 1. Some
  // models (Divider, Clusters) rise between ticks: those are rounded to the
  // nearest step.
  bool reset = true;
  bool previous[2] = { false, false };
  const int total = (num_steps + 1) * kSamplesPerStep;
  for (int done = 0; done < total; done += kBlock) {
    generator->Process(false, &reset, nullptr, ramps, gates, kBlock);
    reset = false;
    for (size_t i = 0; i < kBlock; ++i) {
      for (int channel = 0; channel < 2; ++channel) {
        bool gate = gates[2 * i + channel];
        if (gate && !previous[channel]) {
          int step = (done + static_cast<int>(i) + kSamplesPerStep / 2) / kSamplesPerStep - 1;
          if (step >= 0 && step < num_steps) {
            (channel == 0 ? t1 : t3)[step] = true;
          }
        }
        previous[channel] = gate;
      }
    }
  }
}

}  // namespace drumfake
