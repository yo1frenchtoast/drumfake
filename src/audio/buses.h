// Block size, sample rate and the mix buses shared by tracks and the master.
//
// Kept free of Mutable Instruments headers: plaits/resources.h and
// rings/resources.h define the same macros with different values, so the two
// modules must never be included in the same translation unit.

#ifndef DRUMFAKE_AUDIO_BUSES_H_
#define DRUMFAKE_AUDIO_BUSES_H_

#include <cstddef>

namespace drumfake {

const float kSampleRate = 48000.0f;  // Same as plaits::kSampleRate.
const size_t kBlockSize = 12;        // Same as plaits::kBlockSize.

struct Buses {
  float dry_l[kBlockSize];
  float dry_r[kBlockSize];
  float reverb_l[kBlockSize];
  float reverb_r[kBlockSize];
  float delay_l[kBlockSize];
  float delay_r[kBlockSize];

  void Clear();
};

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_BUSES_H_
