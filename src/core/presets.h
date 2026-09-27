// Factory sounds, selected with the "Sounds" mode and pads 1-8 (2 banks).

#ifndef DRUMFREAK_CORE_PRESETS_H_
#define DRUMFREAK_CORE_PRESETS_H_

#include "core/project.h"

namespace drumfreak {

const int kNumPresets = 16;

enum PresetId {
  PRESET_KICK_808,
  PRESET_KICK_909,
  PRESET_SNARE_808,
  PRESET_SNARE_909,
  PRESET_CLOSED_HAT,
  PRESET_OPEN_HAT,
  PRESET_CLAP,
  PRESET_LOW_TOM,
  PRESET_RIM,
  PRESET_COWBELL,
  PRESET_METAL,
  PRESET_PLUCK,
  PRESET_ACID_BASS,
  PRESET_CHORD_STAB,
  PRESET_CHIP_BLEEP,
  PRESET_SPEECH,
};

const char* preset_name(int preset);
void ApplyPreset(int preset, TrackSound* sound);

}  // namespace drumfreak

#endif  // DRUMFREAK_CORE_PRESETS_H_
