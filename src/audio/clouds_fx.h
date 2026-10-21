// Mutable Instruments Clouds (granular, stretch, looping delay, spectral) as
// an effect on the master mix.
//
// Kept free of Clouds headers: clouds/resources.h defines LUT_* macros that
// clash with the other modules, so Clouds lives in clouds_fx.cc.
//
// Clouds runs directly at the engine's 48 kHz instead of its native 32 kHz:
// grain and buffer times are 2/3 of the module's, pitch is unaffected. The
// recording buffers are enlarged to keep a couple of seconds of audio.

#ifndef DRUMFAKE_AUDIO_CLOUDS_FX_H_
#define DRUMFAKE_AUDIO_CLOUDS_FX_H_

#include <cstddef>
#include <memory>

namespace drumfake {

class CloudsFx {
 public:
  CloudsFx();
  ~CloudsFx();

  void Init();

  // Processes `size` frames in place. `params` are the C_* clouds
  // parameters (normalized). At zero dry/wet the effect is skipped once its
  // tail is gone, and the mix is left untouched.
  void Process(float* left, float* right, size_t size, const float* params,
               bool freeze);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_CLOUDS_FX_H_
