// One Drumf(re)ake track: a Plaits voice followed by a filter, drive, sample
// rate reduction, choke gain, volume and pan, with reverb/delay sends.

#ifndef DRUMFREAKE_AUDIO_TRACK_VOICE_H_
#define DRUMFREAKE_AUDIO_TRACK_VOICE_H_

#include <cstddef>

#include "audio/buses.h"
#include "core/params.h"
#include "plaits/dsp/dsp.h"
#include "plaits/dsp/fx/sample_rate_reducer.h"
#include "plaits/dsp/voice.h"
#include "stmlib/dsp/filter.h"

namespace drumfreake {

static_assert(kBlockSize == plaits::kBlockSize, "Block size must match Plaits");

class TrackVoice {
 public:
  void Init();

  // Starts a note. `gate_samples` is the maximum gate length allowed by the
  // sequencer (the gate is also limited by the Gate parameter).
  // A non-zero `gate_samples` replaces the Gate parameter (a step's own gate length).
  void Trigger(float velocity, int note_offset, size_t max_gate_samples,
               size_t gate_samples = 0);
  // Fast fade-out, used by choke groups.
  void Choke();

  // Renders one block of kBlockSize samples. `params` are the effective
  // (locked + modulated) normalized parameters. `extra_delay_send` is added
  // to the delay send (XY "FX" mode).
  void Render(const float* params, float extra_delay_send, Buses* buses);

  // Peak level of the last blocks, for the UI meters.
  float peak() const { return peak_; }
  bool gate_high() const { return last_trigger_high_; }
  // Output of the last block (post-filter, pre-pan), for the scope.
  const float* last_block() const { return mono_; }

 private:
  plaits::Voice voice_;
  char ram_block_[16 * 1024];

  stmlib::Svf filter_;
  plaits::SampleRateReducer crusher_;

  size_t gate_remaining_;
  size_t max_gate_;
  size_t gate_override_;
  bool gate_pending_;
  bool last_trigger_high_;
  float velocity_;
  float note_offset_;
  float filter_envelope_;
  float choke_gain_;
  float choke_target_;
  float peak_;
  float mono_[kBlockSize];
};

}  // namespace drumfreake

#endif  // DRUMFREAKE_AUDIO_TRACK_VOICE_H_
