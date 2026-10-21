// Per-track modulation sources from Mutable Instruments Peaks: an AD
// envelope and a bouncing ball, both restarted by every note.
//
// Kept free of Peaks headers: peaks/resources.h defines LUT_* macros that
// clash with plaits/resources.h, so the Peaks code lives in mod_sources.cc
// behind a pointer to an opaque implementation.

#ifndef DRUMFAKE_AUDIO_MOD_SOURCES_H_
#define DRUMFAKE_AUDIO_MOD_SOURCES_H_

#include <memory>

#include "audio/buses.h"

namespace drumfake {

class ModSources {
 public:
  ModSources();
  ~ModSources();

  // Restarts both sources on the next Process().
  void Trigger();

  // Advances both sources by one block. The parameters are normalized:
  // envelope attack and decay time, ball gravity and bounce.
  void Process(float attack, float decay, float gravity, float bounce);

  // Values after the last block, both in [0, 1].
  float envelope() const { return envelope_; }
  float ball() const { return ball_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  float envelope_;
  float ball_;
};

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_MOD_SOURCES_H_
