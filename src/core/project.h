// Data model: sounds, steps, patterns and the whole project.
//
// The model is plain data. It is written by the UI thread (under the engine
// lock) and read by the audio thread.

#ifndef DRUMFAKE_CORE_PROJECT_H_
#define DRUMFAKE_CORE_PROJECT_H_

#include <cmath>
#include <cstdint>
#include <string>

#include "core/marbles_gen.h"
#include "core/params.h"
#include "core/scales.h"
#include "core/slicing.h"

namespace drumfake {

const int kMaxSteps = 64;
const int kStepsPerPage = 16;
const int kNumPages = kMaxSteps / kStepsPerPage;
const int kNumPatterns = 16;
const int kMaxRatchets = 4;
const int kMaxSongLength = 64;
// The sampler's pool: numbered slots holding files of the project's "samples"
// directory. A track on the Sample engine picks its slot with the Aux knob.
const int kNumSampleSlots = 128;

// Time division of a pattern: the length of one step, in 16th notes.
const int kNumDivisions = 5;
const int kDefaultDivision = 2;  // 1/16.
extern const float kDivisionSteps[kNumDivisions];
extern const char* const kDivisionNames[kNumDivisions];
const float kMaxMicroTiming = 0.5f;  // Of a step, either way.
const float kMaxStepGate = 4.0f;     // Longest step gate, in steps (ties over the next ones).

struct Step {
  bool on;
  float velocity;     // 0 to 1.
  float probability;  // 0 to 1.
  int ratchets;       // 1 to kMaxRatchets.
  int note;           // Semitones relative to the track pitch.
  float micro;        // Timing offset, in steps (-0.5 to 0.5).
  float gate;         // Gate length, in steps (of a ratchet); 0 = the Gate parameter.
  bool slide;         // Glide the parameters into this step's locks instead of jumping.
  // A chord: the notes besides `note`, in semitones above it (the voices of the engine
  // play them with the track's own sound).
  uint8_t chord_size;
  int8_t chord[kMaxChordNotes - 1];
  float locks[kNumLockable];  // NAN when the parameter is not locked.

  void Clear();
  bool has_lock(int param) const { return !std::isnan(locks[param]); }
  bool has_any_lock() const;
  void ClearLocks();
};

struct TrackSequence {
  Step steps[kMaxSteps];
  int length;  // 0 = follow the pattern length.

  void Clear();
  bool IsEmpty() const;
  // Removes the locks of `param` from every step. Returns how many steps had one.
  int ClearLocks(int param);
};

struct TrackSound {
  std::string name;
  float params[kNumTrackParams];

  void Reset();  // All parameters to their defaults.
};

struct Pattern {
  TrackSequence tracks[kNumTracks];
  // The pattern's sounds, loaded with it (unless the engine is in Panel mode).
  TrackSound kit[kNumTracks];
  int length;   // In steps, 1 to kMaxSteps.
  float swing;  // 0.5 (straight) to 0.75.
  int division; // Index in kDivisionSteps.
  // Polyrhythm: tracks with their own length keep cycling across pattern
  // loops instead of restarting with the pattern.
  bool poly;
  // Vary: variations applied while the pattern plays, never written into its
  // steps (velocity, dropped and ghost hits, timing, extra ratchets, decay).
  // `vary` is the amount (0 = none); each step's variation is drawn from
  // `vary_seed`, so it is the same on every loop until the next redraw.
  float vary;
  uint32_t vary_seed;

  void Clear();
  bool IsEmpty() const;
  // With destination > source: repeats the steps of pages 0 to `source` (all
  // tracks, locks included) up to the end of page `destination`, and extends
  // the pattern to it. Otherwise copies page `source` onto page `destination`.
  void CopyPages(int source, int destination);
  // Shift + page: sets the length to `new_length` steps,
  // repeating the current steps (locks included) when it grows. With `track`
  // < 0 it is the pattern's length, and the tracks that follow it are repeated
  // (tracks with their own length keep it); otherwise it is the length of that
  // track alone.
  void Extend(int track, int new_length);
  int track_length(int track) const {
    int l = tracks[track].length;
    return l > 0 ? l : length;
  }
};

struct Project {
  // The kit playing and being edited: a live copy of the current pattern's
  // kit, or a free-standing "panel" kit in Panel mode (see Engine::SetPanel).
  TrackSound sounds[kNumTracks];
  Pattern patterns[kNumPatterns];
  float master[M_LAST];
  float assign[A_LAST];
  float clouds[C_LAST];
  float bpm;
  float volume;
  // Keys and Chords pad modes: root (0 = C), scale and octave shift, for the whole
  // project, and the kind of chord the pads of the Chords mode play.
  int key_root;
  int key_scale;
  int key_octave;
  int chord_type;
  // MIDI input. Each track listens on its own channel (1-16, 0 = none) and
  // plays the notes it receives at their pitch. On the other channels, the
  // eight notes of a major scale from `midi_trigger_note` trigger tracks A-H
  // at their own pitch. `midi_clock_sync` follows an external clock.
  bool midi_clock_sync;
  // Pattern changes wait for the end of the playing pattern ("At End"), or
  // happen at once.
  bool switch_at_end;
  int midi_channels[kNumTracks];
  int midi_trigger_note;
  // Marbles random rhythm: settings, and the tracks receiving the two outputs.
  MarblesSettings marbles;
  int marbles_tracks[2];
  // Song: chain of pattern indices, played in order (a pattern may repeat).
  int song[kMaxSongLength];
  int song_length;
  // Sample pool: file names in the samples directory ("" = empty slot), and
  // the slices of each sample.
  std::string samples[kNumSampleSlots];
  SampleSlices slices[kNumSampleSlots];

  // Default kit and demo pattern.
  void Init();
};

// Builds the demo groove in `pattern`.
void MakeDemoPattern(Pattern* pattern);

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_PROJECT_H_
