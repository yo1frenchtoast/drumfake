// The Sample engine: plays a sample of the pool, in place of the Plaits voice
// of a track. The knobs of the Sound page, as on hybrid synths' sample
// oscillators:
// - Harmonics: Start, from the beginning (0) to the end (1) of the sample;
// - Timbre: Length, bipolar: up to the whole sample after Start (1), or before
//   it, played backwards (0); the middle plays nothing;
// - Morph: Loop. 0 plays once; above, the part from this point (between Start
//   and the end of Length) to the end loops, with a crossfade;
// - Decay: release after the gate closes; fully up, the sample plays to its end;
// - Aux: the slot of the pool (see core/sample_pool.h);
// - Colour: the slice, when the sample is sliced (fully down: the whole
//   sample); Start, Length and Loop then apply within the slice;
// - Pitch and the note: C4 plays the sample at its own speed.

#ifndef DRUMFAKE_AUDIO_SAMPLE_PLAYER_H_
#define DRUMFAKE_AUDIO_SAMPLE_PLAYER_H_

#include "audio/buses.h"
#include "core/sample_file.h"
#include "core/slicing.h"

namespace drumfake {

static_assert(kSampleFileRate == static_cast<int>(kSampleRate),
              "samples are kept at the engine's rate");

const float kSampleRootNote = 60.0f;  // Plays at the sample's own speed.

class SamplePlayer {
 public:
  void Init();
  // Starts over from Start on the next Render.
  void Start();
  // Renders kBlockSize frames. `params` are the track's effective parameters,
  // `note` the MIDI note (Pitch knob and note offset). `sample` may change
  // between blocks (or be null): the position carries over, clamped.
  void Render(const SampleData* sample, const SampleSlices* slices, const float* params,
              float note, bool gate, float* out);
  // Playing, or about to start.
  bool active() const { return playing_ || pending_; }

 private:
  bool pending_;
  bool playing_;
  double played_;   // Frames played from Start, along the direction of Length.
  float envelope_;  // Release after the gate.
  int fade_in_;     // Frames left of the declick ramp at the start.
};

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_SAMPLE_PLAYER_H_
