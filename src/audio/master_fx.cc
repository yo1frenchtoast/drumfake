#include "audio/master_fx.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/params.h"
#include "rings/dsp/fx/chorus.h"
#include "rings/dsp/fx/reverb.h"

namespace drumfreake {

void Buses::Clear() {
  memset(this, 0, sizeof(*this));
}

namespace {

// Longest delay: a half note at 30 BPM.
const size_t kDelayLength = static_cast<size_t>(4.2f * 48000.0f);

inline float ReadDelay(const std::vector<float>& line, size_t write, float delay) {
  float position = static_cast<float>(write) - delay;
  while (position < 0.0f) {
    position += static_cast<float>(line.size());
  }
  size_t i = static_cast<size_t>(position);
  float frac = position - static_cast<float>(i);
  float a = line[i % line.size()];
  float b = line[(i + 1) % line.size()];
  return a + (b - a) * frac;
}

}  // namespace

MasterFx::MasterFx()
    : reverb_(new rings::Reverb()),
      chorus_(new rings::Chorus()) {
}

MasterFx::~MasterFx() { }

void MasterFx::Init() {
  reverb_->Init(reverb_buffer_);
  reverb_->set_diffusion(0.625f);
  reverb_->set_input_gain(0.2f);
  reverb_->set_amount(1.0f);
  chorus_->Init(chorus_buffer_);
  delay_l_.assign(kDelayLength, 0.0f);
  delay_r_.assign(kDelayLength, 0.0f);
  delay_write_ = 0;
  delay_time_ = 0.0f;
  delay_damp_l_ = delay_damp_r_ = 0.0f;
  dj_l_.Init();
  dj_r_.Init();
}

void MasterFx::Process(const float* params, float bpm, float volume,
                       float dj_filter, Buses* b, float* out_l, float* out_r) {
  const size_t n = kBlockSize;

  // Reverb: in place on the send bus, 100% wet.
  reverb_->set_time(0.5f + 0.49f * params[M_REVERB_SIZE]);
  reverb_->set_lp(0.3f + 0.6f * params[M_REVERB_TONE]);
  reverb_->Process(b->reverb_l, b->reverb_r, n);

  // Ping-pong delay with damping in the feedback path.
  float quarter = 60.0f / bpm * kSampleRate;
  float target = kDelayTimes[Discrete(params[M_DELAY_TIME], kNumDelayTimes)] * quarter;
  target = std::min(target, static_cast<float>(kDelayLength - 2));
  if (delay_time_ == 0.0f) {
    delay_time_ = target;
  }
  const float feedback = params[M_DELAY_FEEDBACK] * 0.95f;
  const float delay_level = params[M_DELAY_LEVEL];
  const float reverb_level = params[M_REVERB_LEVEL] * 1.5f;
  for (size_t i = 0; i < n; ++i) {
    delay_time_ += (target - delay_time_) * 0.0005f;
    float tap_l = ReadDelay(delay_l_, delay_write_, delay_time_);
    float tap_r = ReadDelay(delay_r_, delay_write_, delay_time_);
    delay_damp_l_ += (tap_l - delay_damp_l_) * 0.5f;
    delay_damp_r_ += (tap_r - delay_damp_r_) * 0.5f;
    float in = 0.5f * (b->delay_l[i] + b->delay_r[i]);
    delay_l_[delay_write_] = in + delay_damp_r_ * feedback;
    delay_r_[delay_write_] = delay_damp_l_ * feedback;
    delay_write_ = (delay_write_ + 1) % kDelayLength;
    out_l[i] = b->dry_l[i] + b->reverb_l[i] * reverb_level + tap_l * delay_level;
    out_r[i] = b->dry_r[i] + b->reverb_r[i] * reverb_level + tap_r * delay_level;
  }

  if (params[M_CHORUS] > 0.01f) {
    chorus_->set_amount(params[M_CHORUS]);
    chorus_->set_depth(0.3f + 0.4f * params[M_CHORUS]);
    chorus_->Process(out_l, out_r, n);
  }

  // DJ filter: low pass to the left of the center, high pass to the right.
  if (std::fabs(dj_filter) > 0.02f) {
    float amount = std::fabs(dj_filter);
    float f = dj_filter < 0.0f
        ? 20000.0f * powf(2.0f, -8.0f * amount)
        : 20.0f * powf(2.0f, 8.0f * amount);
    f = std::min(f / kSampleRate, 0.49f);
    dj_l_.set_f_q<stmlib::FREQUENCY_EXACT>(f, 1.2f);
    dj_r_.set(dj_l_);
    if (dj_filter < 0.0f) {
      dj_l_.Process<stmlib::FILTER_MODE_LOW_PASS>(out_l, out_l, n);
      dj_r_.Process<stmlib::FILTER_MODE_LOW_PASS>(out_r, out_r, n);
    } else {
      dj_l_.Process<stmlib::FILTER_MODE_HIGH_PASS>(out_l, out_l, n);
      dj_r_.Process<stmlib::FILTER_MODE_HIGH_PASS>(out_r, out_r, n);
    }
  }

  // Drive, then a soft limiter.
  const float drive = 1.0f + 6.0f * params[M_DRIVE] * params[M_DRIVE];
  const float makeup = 1.0f / sqrtf(drive);
  const float gain = volume * volume * 1.5f;
  for (size_t i = 0; i < n; ++i) {
    out_l[i] = tanhf(tanhf(out_l[i] * drive) * makeup * gain);
    out_r[i] = tanhf(tanhf(out_r[i] * drive) * makeup * gain);
  }
}

}  // namespace drumfreake
