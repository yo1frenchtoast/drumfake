// Parameter definitions shared by the model, the audio engine and the UI.
//
// Every parameter is stored normalized in [0, 1]. The audio engine maps the
// normalized value to real units; the UI uses ParamInfo to print it.

#ifndef DRUMFREAKE_CORE_PARAMS_H_
#define DRUMFREAKE_CORE_PARAMS_H_

#include <string>

namespace drumfreake {

const int kNumTracks = 8;
const int kKnobsPerPage = 8;

// Pages selected by the buttons under the "Sound Edit" knobs.
enum Page {
  PAGE_SOUND,
  PAGE_FILTER,
  PAGE_TRACK,
  PAGE_MASTER,
  PAGE_MODS,
  PAGE_ENVS,
  PAGE_CLOUDS,
  PAGE_ASSIGN,
  PAGE_LAST
};

// Per-track parameters. The first kNumLockable ones can be parameter-locked
// per step and targeted by modulations.
enum TrackParam {
  // Sound page.
  P_ENGINE,
  P_PITCH,
  P_HARMONICS,
  P_TIMBRE,
  P_MORPH,
  P_DECAY,
  P_COLOUR,
  P_AUX,
  // Filter page.
  P_CUTOFF,
  P_RESONANCE,
  P_FILTER_MODE,
  P_FILTER_ENV,
  P_FILTER_DECAY,
  P_DRIVE,
  P_CRUSH,
  P_GATE,
  // Track page.
  P_VOLUME,
  P_PAN,
  P_REVERB_SEND,
  P_DELAY_SEND,
  P_FM_ENV,
  P_TIMBRE_ENV,
  P_MORPH_ENV,
  P_CHOKE,
  // Mods page.
  P_LFO1_RATE,
  P_LFO1_SHAPE,
  P_LFO1_DEPTH,
  P_LFO1_DEST,
  P_LFO2_RATE,
  P_LFO2_SHAPE,
  P_LFO2_DEPTH,
  P_LFO2_DEST,
  // Envs page: Peaks modulation sources triggered by every note.
  P_ENV_ATTACK,
  P_ENV_DECAY,
  P_ENV_AMOUNT,
  P_ENV_DEST,
  P_BALL_GRAVITY,
  P_BALL_BOUNCE,
  P_BALL_AMOUNT,
  P_BALL_DEST,
  P_LAST
};

const int kNumTrackParams = P_LAST;
const int kNumLockable = P_LFO1_RATE;

enum MasterParam {
  M_REVERB_SIZE,
  M_REVERB_TONE,
  M_REVERB_LEVEL,
  M_DELAY_TIME,
  M_DELAY_FEEDBACK,
  M_DELAY_LEVEL,
  M_CHORUS,
  M_DRIVE,
  M_LAST
};

enum AssignParam {
  A_MACRO_X_DEST,
  A_MACRO_X_DEPTH,
  A_MACRO_Y_DEST,
  A_MACRO_Y_DEPTH,
  A_GRIDS_DENSITY_1,
  A_GRIDS_DENSITY_2,
  A_GRIDS_DENSITY_3,
  A_GRIDS_CHAOS,
  A_LAST
};

// Clouds, on the master mix (project-wide).
enum CloudsParam {
  C_MODE,
  C_POSITION,
  C_SIZE,
  C_PITCH,
  C_DENSITY,
  C_TEXTURE,
  C_FEEDBACK,
  C_DRY_WET,
  C_LAST
};

enum ParamFormat {
  FORMAT_PERCENT,
  FORMAT_BIPOLAR,
  FORMAT_ENGINE,
  FORMAT_NOTE,
  FORMAT_HZ,
  FORMAT_FILTER_DECAY,
  FORMAT_GATE,
  FORMAT_FILTER_MODE,
  FORMAT_CRUSH,
  FORMAT_CHOKE,
  FORMAT_LFO_RATE,
  FORMAT_LFO_SHAPE,
  FORMAT_DESTINATION,
  FORMAT_DELAY_TIME,
  FORMAT_ENV_TIME,
  FORMAT_CLOUDS_MODE,
  FORMAT_SEMITONES,
};

struct ParamInfo {
  const char* name;
  const char* short_name;
  ParamFormat format;
  float default_value;
};

// Plaits engines, in the order registered by plaits::Voice::Init().
const int kNumEngines = 24;
extern const char* const kEngineNames[kNumEngines];

enum LfoShape {
  LFO_SINE,
  LFO_TRIANGLE,
  LFO_SAW_UP,
  LFO_SAW_DOWN,
  LFO_SQUARE,
  LFO_SAMPLE_HOLD,
  LFO_SHAPE_LAST
};

const int kNumLfoRates = 10;
// LFO period, in quarter notes.
extern const float kLfoPeriods[kNumLfoRates];

const int kNumDelayTimes = 10;
// Delay time, in quarter notes.
extern const float kDelayTimes[kNumDelayTimes];

const int kNumCloudsModes = 4;
const int kNumFilterModes = 3;
const int kNumChokeGroups = 4;
// "Off" followed by every lockable parameter.
const int kNumDestinations = kNumLockable + 1;

const ParamInfo& track_param_info(int param);
const ParamInfo& master_param_info(int param);
const ParamInfo& assign_param_info(int param);
const ParamInfo& clouds_param_info(int param);

// Info for knob `knob` of `page`.
const ParamInfo& page_param_info(Page page, int knob);
const char* page_name(Page page);

std::string FormatValue(ParamFormat format, float value);

// Maps a normalized value to one of n choices, and back.
inline int Discrete(float value, int n) {
  int i = static_cast<int>(value * n);
  return i < 0 ? 0 : (i >= n ? n - 1 : i);
}

inline float DiscreteToNormalized(int i, int n) {
  return (static_cast<float>(i) + 0.5f) / static_cast<float>(n);
}

// Destination index (0 = off) to a lockable parameter, or -1.
inline int DestinationToParam(float value) {
  return Discrete(value, kNumDestinations) - 1;
}

inline float Clamp01(float x) {
  return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

// Normalized value -> real units. Shared by the engine and the display.
float PitchToNote(float value);          // MIDI note, 12 to 108.
float CutoffToHz(float value);           // 20 Hz to 20 kHz.
float FilterDecayToSeconds(float value); // 5 ms to 2 s.
float GateToSeconds(float value);        // 1 ms to 512 ms.
float CrushToRatio(float value);         // 1 (off) down to 1/64.
float NoteToNormalizedPitch(float note);
float CloudsPitchToSemitones(float value);  // -24 to +24, 0 at the centre.
float EnvTimeToSeconds(float value);     // 0.5 ms to 8 s (Peaks envelope times).

}  // namespace drumfreake

#endif  // DRUMFREAKE_CORE_PARAMS_H_
