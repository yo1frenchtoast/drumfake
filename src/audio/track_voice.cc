#include "audio/track_voice.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "stmlib/utils/buffer_allocator.h"

namespace drumfake {

void TrackVoice::Init() {
  stmlib::BufferAllocator allocator(ram_block_, sizeof(ram_block_));
  voice_.Init(&allocator);
  sample_player_.Init();
  filter_.Init();
  crusher_.Init();
  gate_remaining_ = 0;
  gate_pending_ = false;
  last_trigger_high_ = false;
  max_gate_ = 0;
  gate_override_ = 0;
  velocity_ = 0.0f;
  note_offset_ = 0.0f;
  filter_envelope_ = 0.0f;
  choke_gain_ = 1.0f;
  choke_target_ = 1.0f;
  peak_ = 0.0f;
  std::fill(mono_, mono_ + kBlockSize, 0.0f);
}

void TrackVoice::Trigger(float velocity, int note_offset, size_t max_gate_samples,
                         size_t gate_samples) {
  gate_override_ = gate_samples;
  velocity_ = velocity;
  note_offset_ = static_cast<float>(note_offset);
  max_gate_ = std::max(max_gate_samples, kBlockSize);
  gate_pending_ = true;
}

void TrackVoice::Choke() {
  choke_target_ = 0.0f;
  gate_pending_ = false;
  gate_remaining_ = 0;
}

void TrackVoice::Render(const float* params, float extra_delay_send, Buses* buses,
                        const SampleData* sample) {
  // Gate. Plaits detects a trigger on a rising edge, so when a note is
  // retriggered while its gate is still high, the gate goes low for one block.
  if (gate_pending_ && !last_trigger_high_) {
    size_t gate = gate_override_ > 0
        ? gate_override_
        : static_cast<size_t>(GateToSeconds(params[P_GATE]) * kSampleRate);
    gate_remaining_ = std::max(kBlockSize, std::min(gate, max_gate_));
    gate_pending_ = false;
    filter_envelope_ = 1.0f;
    choke_gain_ = choke_target_ = 1.0f;
    sample_player_.Start();
  } else if (gate_pending_) {
    gate_remaining_ = 0;
  }
  bool gate_high = gate_remaining_ > 0;
  gate_remaining_ -= std::min(gate_remaining_, kBlockSize);
  last_trigger_high_ = gate_high;

  const int engine = Discrete(params[P_ENGINE], kNumEngines);
  plaits::Patch patch;
  patch.engine = std::min(engine, kNumPlaitsEngines - 1);
  patch.note = PitchToNote(params[P_PITCH]);
  patch.harmonics = params[P_HARMONICS];
  patch.timbre = params[P_TIMBRE];
  patch.morph = params[P_MORPH];
  patch.decay = params[P_DECAY];
  patch.lpg_colour = params[P_COLOUR];
  patch.frequency_modulation_amount = params[P_FM_ENV] * 2.0f - 1.0f;
  patch.timbre_modulation_amount = params[P_TIMBRE_ENV] * 2.0f - 1.0f;
  patch.morph_modulation_amount = params[P_MORPH_ENV] * 2.0f - 1.0f;

  plaits::Modulations modulations;
  memset(&modulations, 0, sizeof(modulations));
  modulations.note = note_offset_;
  modulations.trigger = gate_high ? 1.0f : 0.0f;
  modulations.level = gate_high ? velocity_ : 0.0f;
  modulations.trigger_patched = true;
  modulations.level_patched = true;

  if (engine == kSampleEngine) {
    sample_player_.Render(sample, params, PitchToNote(params[P_PITCH]) + note_offset_, gate_high,
                          mono_);
  } else {
    plaits::Voice::Frame frames[kBlockSize];
    voice_.Render(patch, modulations, frames, kBlockSize);
    const float aux = params[P_AUX];
    for (size_t i = 0; i < kBlockSize; ++i) {
      float out = static_cast<float>(frames[i].out) / 32768.0f;
      float aux_out = static_cast<float>(frames[i].aux) / 32768.0f;
      mono_[i] = out + (aux_out - out) * aux;
    }
  }

  // Filter, with an exponentially decaying envelope on the cutoff.
  const float envelope_amount = params[P_FILTER_ENV] * 2.0f - 1.0f;
  const int mode = Discrete(params[P_FILTER_MODE], kNumFilterModes);
  const bool bypass_filter = mode == 0 && params[P_CUTOFF] > 0.99f &&
      std::fabs(envelope_amount) < 0.02f;
  if (!bypass_filter) {
    float cutoff = CutoffToHz(params[P_CUTOFF]) *
        powf(2.0f, 6.0f * envelope_amount * filter_envelope_);
    float f = std::min(cutoff / kSampleRate, 0.49f);
    float resonance = params[P_RESONANCE];
    filter_.set_f_q<stmlib::FREQUENCY_EXACT>(f, 0.5f + 20.0f * resonance * resonance);
    switch (mode) {
      case 0:
        filter_.Process<stmlib::FILTER_MODE_LOW_PASS>(mono_, mono_, kBlockSize);
        break;
      case 1:
        filter_.Process<stmlib::FILTER_MODE_BAND_PASS_NORMALIZED>(mono_, mono_, kBlockSize);
        break;
      default:
        filter_.Process<stmlib::FILTER_MODE_HIGH_PASS>(mono_, mono_, kBlockSize);
        break;
    }
  }
  filter_envelope_ *= expf(
      -static_cast<float>(kBlockSize) /
      (FilterDecayToSeconds(params[P_FILTER_DECAY]) * kSampleRate));

  const float drive = params[P_DRIVE];
  if (drive > 0.01f) {
    float gain = 1.0f + 15.0f * drive * drive;
    float makeup = 1.0f / sqrtf(gain);
    for (size_t i = 0; i < kBlockSize; ++i) {
      mono_[i] = tanhf(mono_[i] * gain) * makeup;
    }
  }

  float crush = CrushToRatio(params[P_CRUSH]);
  if (crush < 1.0f) {
    crusher_.Process<false>(crush, mono_, kBlockSize);
  }

  // Choke (about 3 ms fade), velocity, volume and pan.
  const float choke_coefficient = 0.015f;
  const float volume = params[P_VOLUME] * params[P_VOLUME] *
      (0.35f + 0.65f * velocity_);
  const float pan = params[P_PAN] * 1.5707963f;
  const float gain_l = volume * cosf(pan) * 1.4142135f;
  const float gain_r = volume * sinf(pan) * 1.4142135f;
  const float reverb_send = params[P_REVERB_SEND];
  const float delay_send = std::min(params[P_DELAY_SEND] + extra_delay_send, 1.0f);
  float peak = peak_ * 0.98f;
  for (size_t i = 0; i < kBlockSize; ++i) {
    choke_gain_ += (choke_target_ - choke_gain_) * choke_coefficient;
    float s = mono_[i] * choke_gain_;
    mono_[i] = s;
    float l = s * gain_l;
    float r = s * gain_r;
    buses->dry_l[i] += l;
    buses->dry_r[i] += r;
    buses->reverb_l[i] += l * reverb_send;
    buses->reverb_r[i] += r * reverb_send;
    buses->delay_l[i] += l * delay_send;
    buses->delay_r[i] += r * delay_send;
    peak = std::max(peak, std::fabs(s * volume));
  }
  peak_ = peak;
}

}  // namespace drumfake
