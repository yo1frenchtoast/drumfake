#include "core/params.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace drumfake {

const char* const kEngineNames[kNumEngines] = {
  "VA + VCF",
  "Phase Dist",
  "6-Op FM 1",
  "6-Op FM 2",
  "6-Op FM 3",
  "Wave Terrain",
  "String Mach",
  "Chiptune",
  "Virt Analog",
  "Waveshaper",
  "2-Op FM",
  "Formant",
  "Additive",
  "Wavetable",
  "Chords",
  "Speech",
  "Swarm",
  "Filt Noise",
  "Particles",
  "Inharm Str",
  "Modal",
  "Bass Drum",
  "Snare Drum",
  "Hi-Hat",
  "Sample",
  "Scrub",
  "Spray",
  "Burst",
};

const float kLfoPeriods[kNumLfoRates] = {
  64.0f, 32.0f, 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f
};

static const char* const kLfoRateNames[kNumLfoRates] = {
  "16 bars", "8 bars", "4 bars", "2 bars", "1 bar",
  "1/2", "1/4", "1/8", "1/16", "1/32"
};

const float kDelayTimes[kNumDelayTimes] = {
  0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f,
  0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 2.0f
};

static const char* const kDelayTimeNames[kNumDelayTimes] = {
  "1/32", "1/16T", "1/16", "1/8T", "1/16D",
  "1/8", "1/4T", "1/8D", "1/4", "1/2"
};

static const char* const kLfoShapeNames[LFO_SHAPE_LAST] = {
  "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H"
};

static const char* const kCloudsModeNames[kNumCloudsModes] = {
  "Granular", "Stretch", "Looping", "Spectral"
};

static const char* const kFilterModeNames[kNumFilterModes] = {
  "Low Pass", "Band Pass", "High Pass"
};

static const ParamInfo kTrackParams[kNumTrackParams] = {
  { "Engine", "Eng", FORMAT_ENGINE, 0.9f },
  { "Pitch", "Ptch", FORMAT_NOTE, 0.375f },
  { "Harmonics", "Harm", FORMAT_PERCENT, 0.5f },
  { "Timbre", "Timb", FORMAT_PERCENT, 0.5f },
  { "Morph", "Mrph", FORMAT_PERCENT, 0.5f },
  { "Decay", "Dcy", FORMAT_PERCENT, 0.5f },
  { "LPG Colour", "Col", FORMAT_PERCENT, 0.5f },
  { "Out/Aux", "Aux", FORMAT_PERCENT, 0.0f },

  { "Cutoff", "Cut", FORMAT_HZ, 1.0f },
  { "Resonance", "Res", FORMAT_PERCENT, 0.0f },
  { "Filter Mode", "Mode", FORMAT_FILTER_MODE, 0.1f },
  { "Filter Env", "FEnv", FORMAT_BIPOLAR, 0.5f },
  { "Env Decay", "FDcy", FORMAT_FILTER_DECAY, 0.4f },
  { "Drive", "Drv", FORMAT_PERCENT, 0.0f },
  { "Crush", "Crsh", FORMAT_CRUSH, 0.0f },
  { "Gate", "Gate", FORMAT_GATE, 0.3f },

  { "Volume", "Vol", FORMAT_PERCENT, 0.8f },
  { "Pan", "Pan", FORMAT_BIPOLAR, 0.5f },
  { "Reverb Send", "Rev", FORMAT_PERCENT, 0.0f },
  { "Delay Send", "Dly", FORMAT_PERCENT, 0.0f },
  { "Pitch Env", "PEnv", FORMAT_BIPOLAR, 0.5f },
  { "Timbre Env", "TEnv", FORMAT_BIPOLAR, 0.5f },
  { "Morph Env", "MEnv", FORMAT_BIPOLAR, 0.5f },
  { "Choke Group", "Chk", FORMAT_CHOKE, 0.0f },

  { "LFO1 Rate", "L1Rt", FORMAT_LFO_RATE, 0.45f },
  { "LFO1 Shape", "L1Sh", FORMAT_LFO_SHAPE, 0.0f },
  { "LFO1 Depth", "L1Dp", FORMAT_BIPOLAR, 0.5f },
  { "LFO1 Dest", "L1Ds", FORMAT_DESTINATION, 0.0f },
  { "LFO2 Rate", "L2Rt", FORMAT_LFO_RATE, 0.65f },
  { "LFO2 Shape", "L2Sh", FORMAT_LFO_SHAPE, 0.0f },
  { "LFO2 Depth", "L2Dp", FORMAT_BIPOLAR, 0.5f },
  { "LFO2 Dest", "L2Ds", FORMAT_DESTINATION, 0.0f },

  { "Env Attack", "EAtk", FORMAT_ENV_TIME, 0.1f },
  { "Env Decay", "EDcy", FORMAT_ENV_TIME, 0.55f },
  { "Env Amount", "EAmt", FORMAT_BIPOLAR, 0.75f },
  { "Env Dest", "EDst", FORMAT_DESTINATION, 0.0f },
  { "Ball Gravity", "BGrv", FORMAT_PERCENT, 0.5f },
  { "Ball Bounce", "BBnc", FORMAT_PERCENT, 0.5f },
  { "Ball Amount", "BAmt", FORMAT_BIPOLAR, 0.75f },
  { "Ball Dest", "BDst", FORMAT_DESTINATION, 0.0f },
};

static const ParamInfo kMasterParams[M_LAST] = {
  { "Reverb Size", "Size", FORMAT_PERCENT, 0.6f },
  { "Reverb Tone", "Tone", FORMAT_PERCENT, 0.6f },
  { "Reverb Level", "RLvl", FORMAT_PERCENT, 0.7f },
  { "Delay Time", "Time", FORMAT_DELAY_TIME, 0.75f },
  { "Delay Fdbk", "Fdbk", FORMAT_PERCENT, 0.4f },
  { "Delay Level", "DLvl", FORMAT_PERCENT, 0.6f },
  { "Chorus", "Chor", FORMAT_PERCENT, 0.0f },
  { "Drive", "Drv", FORMAT_PERCENT, 0.1f },
};

static const ParamInfo kAssignParams[A_LAST] = {
  { "Macro X Dest", "XDst", FORMAT_DESTINATION, 0.0f },
  { "Macro X Depth", "XDpt", FORMAT_BIPOLAR, 1.0f },
  { "Macro Y Dest", "YDst", FORMAT_DESTINATION, 0.0f },
  { "Macro Y Depth", "YDpt", FORMAT_BIPOLAR, 1.0f },
  { "Grids Fill A", "FilA", FORMAT_PERCENT, 0.5f },
  { "Grids Fill B", "FilB", FORMAT_PERCENT, 0.5f },
  { "Grids Fill C", "FilC", FORMAT_PERCENT, 0.5f },
  { "Grids Chaos", "Chao", FORMAT_PERCENT, 0.0f },
};

static const ParamInfo kCloudsParams[C_LAST] = {
  { "Clouds Mode", "Mode", FORMAT_CLOUDS_MODE, 0.0f },
  { "Position", "Pos", FORMAT_PERCENT, 0.5f },
  { "Size", "Size", FORMAT_PERCENT, 0.5f },
  { "Pitch", "Ptch", FORMAT_SEMITONES, 0.5f },
  { "Density", "Dens", FORMAT_PERCENT, 0.75f },
  { "Texture", "Text", FORMAT_PERCENT, 0.5f },
  { "Feedback", "Fdbk", FORMAT_PERCENT, 0.0f },
  { "Dry/Wet", "D/W", FORMAT_PERCENT, 0.0f },
};

static const char* const kPageNames[PAGE_LAST] = {
  "Sound", "Filter", "Track", "Master", "Mods", "Envs", "Clouds", "Assign"
};

const ParamInfo& track_param_info(int param) {
  return kTrackParams[param];
}

const ParamInfo& master_param_info(int param) {
  return kMasterParams[param];
}

const ParamInfo& clouds_param_info(int param) {
  return kCloudsParams[param];
}

const ParamInfo& assign_param_info(int param) {
  return kAssignParams[param];
}

const ParamInfo& page_param_info(Page page, int knob) {
  switch (page) {
    case PAGE_SOUND: return kTrackParams[P_ENGINE + knob];
    case PAGE_FILTER: return kTrackParams[P_CUTOFF + knob];
    case PAGE_TRACK: return kTrackParams[P_VOLUME + knob];
    case PAGE_MODS: return kTrackParams[P_LFO1_RATE + knob];
    case PAGE_ENVS: return kTrackParams[P_ENV_ATTACK + knob];
    case PAGE_CLOUDS: return kCloudsParams[knob];
    case PAGE_MASTER: return kMasterParams[knob];
    case PAGE_ASSIGN:
    default: return kAssignParams[knob];
  }
}

const char* page_name(Page page) {
  return kPageNames[page];
}

float PitchToNote(float value) {
  return 12.0f + 96.0f * value;
}

float NoteToNormalizedPitch(float note) {
  return Clamp01((note - 12.0f) / 96.0f);
}

float CutoffToHz(float value) {
  return 20.0f * powf(2.0f, 10.0f * value);
}

float FilterDecayToSeconds(float value) {
  return 0.005f * powf(400.0f, value);
}

float GateToSeconds(float value) {
  return 0.001f * powf(2.0f, 9.0f * value);
}

float GrainReleaseToSeconds(float value) {
  return 0.005f * powf(1600.0f, value);
}

float CloudsPitchToSemitones(float value) {
  return (Clamp01(value) - 0.5f) * 48.0f;
}

float EnvTimeToSeconds(float value) {
  // Peaks' lut_env_increments sampled every 64 entries (of 256), interpolated
  // geometrically: close enough for a display.
  static const float kSeconds[5] = { 0.0005f, 0.0357f, 0.3996f, 2.171f, 8.0f };
  float position = Clamp01(value) * 4.0f;
  int i = std::min(static_cast<int>(position), 3);
  return kSeconds[i] * powf(kSeconds[i + 1] / kSeconds[i], position - i);
}

float CrushToRatio(float value) {
  return value < 0.01f ? 1.0f : powf(2.0f, -6.0f * value);
}

std::string FormatValue(ParamFormat format, float value) {
  static const char* const kNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
  };
  char buffer[32];
  switch (format) {
    case FORMAT_PERCENT:
      snprintf(buffer, sizeof(buffer), "%d%%", static_cast<int>(value * 100.0f + 0.5f));
      break;
    case FORMAT_BIPOLAR:
      snprintf(buffer, sizeof(buffer), "%+d%%",
               static_cast<int>(roundf((value * 2.0f - 1.0f) * 100.0f)));
      break;
    case FORMAT_ENGINE:
      return kEngineNames[Discrete(value, kNumEngines)];
    case FORMAT_NOTE: {
      int note = static_cast<int>(roundf(PitchToNote(value)));
      snprintf(buffer, sizeof(buffer), "%s%d", kNoteNames[note % 12], note / 12 - 1);
      break;
    }
    case FORMAT_HZ: {
      float hz = CutoffToHz(value);
      if (hz >= 1000.0f) {
        snprintf(buffer, sizeof(buffer), "%.1fkHz", hz / 1000.0f);
      } else {
        snprintf(buffer, sizeof(buffer), "%dHz", static_cast<int>(hz));
      }
      break;
    }
    case FORMAT_FILTER_DECAY:
    case FORMAT_ENV_TIME:
    case FORMAT_GATE: {
      float seconds = format == FORMAT_GATE
          ? GateToSeconds(value)
          : (format == FORMAT_ENV_TIME ? EnvTimeToSeconds(value)
                                       : FilterDecayToSeconds(value));
      if (seconds >= 1.0f) {
        snprintf(buffer, sizeof(buffer), "%.2fs", seconds);
      } else {
        snprintf(buffer, sizeof(buffer), "%dms", static_cast<int>(seconds * 1000.0f + 0.5f));
      }
      break;
    }
    case FORMAT_CLOUDS_MODE:
      return kCloudsModeNames[Discrete(value, kNumCloudsModes)];
    case FORMAT_SEMITONES:
      snprintf(buffer, sizeof(buffer), "%+.1f st", CloudsPitchToSemitones(value));
      break;
    case FORMAT_FILTER_MODE:
      return kFilterModeNames[Discrete(value, kNumFilterModes)];
    case FORMAT_CRUSH: {
      float ratio = CrushToRatio(value);
      if (ratio >= 1.0f) {
        return "Off";
      }
      snprintf(buffer, sizeof(buffer), "%.1fkHz", 48.0f * ratio);
      break;
    }
    case FORMAT_CHOKE: {
      int group = Discrete(value, kNumChokeGroups);
      if (group == 0) {
        return "Off";
      }
      snprintf(buffer, sizeof(buffer), "Group %d", group);
      break;
    }
    case FORMAT_LFO_RATE:
      return kLfoRateNames[Discrete(value, kNumLfoRates)];
    case FORMAT_LFO_SHAPE:
      return kLfoShapeNames[Discrete(value, LFO_SHAPE_LAST)];
    case FORMAT_DESTINATION: {
      int param = DestinationToParam(value);
      return param < 0 ? "Off" : kTrackParams[param].name;
    }
    case FORMAT_DELAY_TIME:
      return kDelayTimeNames[Discrete(value, kNumDelayTimes)];
  }
  return buffer;
}

}  // namespace drumfake
