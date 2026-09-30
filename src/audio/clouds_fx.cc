#include "audio/clouds_fx.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "audio/buses.h"
#include "clouds/dsp/frame.h"
#include "clouds/dsp/granular_processor.h"
#include "core/params.h"

namespace drumfreake {

namespace {

// Clouds' buffers: the large one holds the left channel memory plus the
// effects workspace, the small one the right channel memory. The module has
// 64 kB per channel at 32 kHz; 192 kB at 48 kHz keeps about 2 s.
const size_t kSmallBufferSize = 192 * 1024;
const size_t kWorkspaceSize = 64 * 1024;
const size_t kLargeBufferSize = kSmallBufferSize + kWorkspaceSize;

// Blocks to keep processing after the wet level reaches zero, so that tails
// and the dry/wet fade finish.
const int kTailBlocks = 4000;

}  // namespace

struct CloudsFx::Impl {
  clouds::GranularProcessor processor;
  std::vector<uint8_t> large_buffer = std::vector<uint8_t>(kLargeBufferSize);
  std::vector<uint8_t> small_buffer = std::vector<uint8_t>(kSmallBufferSize);
  clouds::ShortFrame input[kBlockSize];
  clouds::ShortFrame output[kBlockSize];
  int tail_blocks = 0;
};

// Value-initialized: GranularProcessor relies on zeroed static memory (the
// module's globals), its Init() leaves flags like `silence_` untouched.
CloudsFx::CloudsFx() : impl_(new Impl()) {}

CloudsFx::~CloudsFx() {}

void CloudsFx::Init() {
  impl_->processor.Init(impl_->large_buffer.data(), kLargeBufferSize,
                        impl_->small_buffer.data(), kSmallBufferSize);
  impl_->processor.set_quality(0);  // Stereo, 16 bits.
  impl_->processor.set_silence(false);
  impl_->processor.set_bypass(false);
  impl_->tail_blocks = 0;
}

void CloudsFx::Process(float* left, float* right, size_t size, const float* params,
                       bool freeze) {
  Impl& impl = *impl_;
  if (params[C_DRY_WET] > 0.001f) {
    impl.tail_blocks = kTailBlocks;
  } else if (impl.tail_blocks > 0) {
    --impl.tail_blocks;
  } else {
    return;
  }

  clouds::GranularProcessor& processor = impl.processor;
  processor.set_playback_mode(static_cast<clouds::PlaybackMode>(
      Discrete(params[C_MODE], clouds::PLAYBACK_MODE_LAST)));
  clouds::Parameters* p = processor.mutable_parameters();
  p->position = params[C_POSITION];
  p->size = params[C_SIZE];
  p->pitch = CloudsPitchToSemitones(params[C_PITCH]);
  p->density = params[C_DENSITY];
  p->texture = params[C_TEXTURE];
  p->feedback = params[C_FEEDBACK];
  p->dry_wet = params[C_DRY_WET];
  p->stereo_spread = 0.0f;
  p->reverb = 0.0f;  // The master has its own reverb.
  p->freeze = freeze;
  p->trigger = false;
  p->gate = false;
  p->granular.stereo_spread = 0.0f;

  auto to_short = [](float x) {
    return static_cast<int16_t>(std::lround(std::min(std::max(x, -1.0f), 1.0f) * 32767.0f));
  };
  for (size_t i = 0; i < size; ++i) {
    impl.input[i].l = to_short(left[i]);
    impl.input[i].r = to_short(right[i]);
  }
  // Clouds splits its work between an audio callback (Process) and a main
  // loop (Prepare: buffer setup, spectral frames, correlator). Both run here,
  // one after the other.
  processor.Prepare();
  processor.Process(impl.input, impl.output, size);
  for (size_t i = 0; i < size; ++i) {
    left[i] = impl.output[i].l / 32768.0f;
    right[i] = impl.output[i].r / 32768.0f;
  }
}

}  // namespace drumfreake
