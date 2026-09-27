// Master section: Rings reverb and a tempo-synced ping-pong delay on the send
// buses, then Rings chorus, a DJ filter, drive and volume on the mix.

#ifndef DRUMFREAKE_AUDIO_MASTER_FX_H_
#define DRUMFREAKE_AUDIO_MASTER_FX_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "audio/buses.h"
#include "stmlib/dsp/filter.h"

namespace rings {
class Chorus;
class Reverb;
}  // namespace rings

namespace drumfreake {

class MasterFx {
 public:
  MasterFx();
  ~MasterFx();

  void Init();

  // `params` are the M_* master parameters. `dj_filter` goes from -1 (low
  // pass) through 0 (off) to 1 (high pass). Writes the stereo mix to out_l
  // and out_r.
  void Process(const float* params, float bpm, float volume, float dj_filter,
               Buses* buses, float* out_l, float* out_r);

 private:
  uint16_t reverb_buffer_[32768];
  uint16_t chorus_buffer_[2048];
  std::unique_ptr<rings::Reverb> reverb_;
  std::unique_ptr<rings::Chorus> chorus_;

  std::vector<float> delay_l_;
  std::vector<float> delay_r_;
  size_t delay_write_;
  float delay_time_;  // Smoothed, in samples.
  float delay_damp_l_;
  float delay_damp_r_;

  stmlib::Svf dj_l_;
  stmlib::Svf dj_r_;
};

}  // namespace drumfreake

#endif  // DRUMFREAKE_AUDIO_MASTER_FX_H_
