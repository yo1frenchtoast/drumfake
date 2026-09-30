#include "audio/mod_sources.h"

#include <algorithm>

#include "peaks/gate_processor.h"
#include "peaks/modulations/bouncing_ball.h"
#include "peaks/modulations/multistage_envelope.h"

namespace drumfreake {

struct ModSources::Impl {
  peaks::MultistageEnvelope envelope;
  peaks::BouncingBall ball;
  bool trigger_pending = false;
  // The ball has no defined position before its first trigger.
  bool ball_started = false;
};

ModSources::ModSources() : impl_(new Impl), envelope_(0.0f), ball_(0.0f) {
  impl_->envelope.Init();
  impl_->ball.Init();
  // The envelope runs one cycle when first configured for AD: let it finish
  // (minimum times) so that it rests at zero until the first trigger.
  uint16_t fastest[2] = { 0, 0 };
  impl_->envelope.Configure(fastest, peaks::CONTROL_MODE_HALF);
  peaks::GateFlags idle[kBlockSize] = {};
  int16_t out[kBlockSize];
  for (int i = 0; i < 16; ++i) {
    impl_->envelope.Process(idle, out, kBlockSize);
  }
}

ModSources::~ModSources() {}

void ModSources::Trigger() {
  impl_->trigger_pending = true;
}

void ModSources::Process(float attack, float decay, float gravity, float bounce) {
  peaks::GateFlags flags[kBlockSize];
  for (size_t i = 0; i < kBlockSize; ++i) {
    flags[i] = peaks::GATE_FLAG_LOW;
  }
  if (impl_->trigger_pending) {
    flags[0] = peaks::GATE_FLAG_RISING | peaks::GATE_FLAG_HIGH;
    for (size_t i = 1; i < kBlockSize; ++i) {
      flags[i] = peaks::GATE_FLAG_HIGH;
    }
    impl_->trigger_pending = false;
    impl_->ball_started = true;
  }

  auto to_u16 = [](float x) {
    return static_cast<uint16_t>(std::min(std::max(x, 0.0f), 1.0f) * 65535.0f);
  };
  int16_t out[kBlockSize];

  // Configure() also resets Init()'s leftover ADSR segment, which set_ad()
  // alone would leave out of range.
  uint16_t envelope_params[2] = { to_u16(attack), to_u16(decay) };
  impl_->envelope.Configure(envelope_params, peaks::CONTROL_MODE_HALF);
  impl_->envelope.Process(flags, out, kBlockSize);
  envelope_ = std::max(0, static_cast<int>(out[kBlockSize - 1])) / 32767.0f;

  if (impl_->ball_started) {
    uint16_t ball_params[4] = { to_u16(gravity), to_u16(bounce), 65535, 32768 };
    impl_->ball.Configure(ball_params, peaks::CONTROL_MODE_FULL);
    impl_->ball.Process(flags, out, kBlockSize);
    ball_ = std::max(0, static_cast<int>(out[kBlockSize - 1])) / 32767.0f;
  }
}

}  // namespace drumfreake
