#include "core/presets.h"

namespace drumfreak {

namespace {

struct Preset {
  const char* name;
  int engine;
  float note;  // MIDI note.
  float harmonics;
  float timbre;
  float morph;
  float decay;
  float colour;
  float aux;
  float gate;
  int choke;
  float reverb;
  float pan;         // -1 to 1.
  float fm_env;      // -1 to 1.
  float timbre_env;  // -1 to 1.
  float cutoff;
  float resonance;
  float filter_env;  // -1 to 1.
  float volume;
};

// Engine numbers follow plaits::Voice::Init(): 21 = bass drum, 22 = snare,
// 23 = hi-hat, 20 = modal, 17 = filtered noise, 10 = 2-op FM...
const Preset kPresets[kNumPresets] = {
  // name          eng note  harm  timb  mrph  dcy   col   aux  gate chk rev  pan   fme   tme   cut   res   fenv  vol
  { "Kick 808",    21, 33.f, 0.30f, 0.40f, 0.55f, 0.5f, 0.5f, 0.0f, 0.3f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.90f },
  { "Kick 909",    21, 36.f, 0.60f, 0.55f, 0.45f, 0.5f, 0.5f, 1.0f, 0.3f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.85f },
  { "Snare 808",   22, 55.f, 0.55f, 0.50f, 0.40f, 0.5f, 0.5f, 0.0f, 0.3f, 0, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.75f },
  { "Snare 909",   22, 52.f, 0.70f, 0.60f, 0.35f, 0.5f, 0.5f, 1.0f, 0.3f, 0, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.75f },
  { "Closed Hat",  23, 72.f, 0.50f, 0.60f, 0.15f, 0.5f, 0.5f, 0.0f, 0.2f, 1, 0.0f, 0.2f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.55f },
  { "Open Hat",    23, 72.f, 0.50f, 0.60f, 0.65f, 0.5f, 0.5f, 0.0f, 0.2f, 1, 0.1f, 0.2f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.50f },
  { "Clap",        17, 62.f, 0.55f, 0.70f, 0.35f, 0.22f, 0.6f, 0.0f, 0.1f, 0, 0.3f, -0.1f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.70f },
  { "Low Tom",     20, 45.f, 0.25f, 0.45f, 0.45f, 0.5f, 0.5f, 0.0f, 0.3f, 0, 0.15f, -0.3f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.75f },
  { "Rim",         20, 79.f, 0.60f, 0.80f, 0.15f, 0.5f, 0.5f, 0.0f, 0.2f, 0, 0.1f, 0.3f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.60f },
  { "Cowbell",     10, 69.f, 0.38f, 0.35f, 0.15f, 0.30f, 0.7f, 0.0f, 0.2f, 0, 0.1f, 0.25f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.50f },
  { "Metal Perc",  20, 64.f, 0.90f, 0.70f, 0.40f, 0.5f, 0.5f, 0.0f, 0.2f, 0, 0.2f, -0.25f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.60f },
  { "Pluck",       19, 57.f, 0.30f, 0.60f, 0.50f, 0.5f, 0.5f, 0.0f, 0.2f, 0, 0.25f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.65f },
  { "Acid Bass",   0,  36.f, 0.55f, 0.30f, 0.60f, 0.35f, 0.3f, 0.0f, 0.5f, 0, 0.0f, 0.0f, 0.0f, 0.6f, 1.0f, 0.0f, 0.0f, 0.70f },
  { "Chord Stab",  14, 60.f, 0.30f, 0.50f, 0.50f, 0.35f, 0.5f, 0.0f, 0.4f, 0, 0.35f, 0.0f, 0.0f, 0.3f, 1.0f, 0.0f, 0.0f, 0.55f },
  { "Chip Bleep",  7,  72.f, 0.40f, 0.50f, 0.30f, 0.30f, 0.5f, 0.0f, 0.4f, 0, 0.2f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.45f },
  { "Speech",      15, 48.f, 0.85f, 0.50f, 0.50f, 0.60f, 0.5f, 0.0f, 0.6f, 0, 0.2f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.70f },
};

float Bipolar(float x) {
  return Clamp01(0.5f + 0.5f * x);
}

}  // namespace

const char* preset_name(int preset) {
  return kPresets[preset].name;
}

void ApplyPreset(int preset, TrackSound* sound) {
  const Preset& p = kPresets[preset];
  sound->Reset();
  sound->name = p.name;
  float* s = sound->params;
  s[P_ENGINE] = DiscreteToNormalized(p.engine, kNumEngines);
  s[P_PITCH] = NoteToNormalizedPitch(p.note);
  s[P_HARMONICS] = p.harmonics;
  s[P_TIMBRE] = p.timbre;
  s[P_MORPH] = p.morph;
  s[P_DECAY] = p.decay;
  s[P_COLOUR] = p.colour;
  s[P_AUX] = p.aux;
  s[P_GATE] = p.gate;
  s[P_CHOKE] = DiscreteToNormalized(p.choke, kNumChokeGroups);
  s[P_REVERB_SEND] = p.reverb;
  s[P_PAN] = Bipolar(p.pan);
  s[P_FM_ENV] = Bipolar(p.fm_env);
  s[P_TIMBRE_ENV] = Bipolar(p.timbre_env);
  s[P_CUTOFF] = p.cutoff;
  s[P_RESONANCE] = p.resonance;
  s[P_FILTER_ENV] = Bipolar(p.filter_env);
  s[P_VOLUME] = p.volume;
}

}  // namespace drumfreak
