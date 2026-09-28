// Data model: sounds, steps, patterns and the whole project.
//
// The model is plain data. It is written by the UI thread (under the engine
// lock) and read by the audio thread.

#ifndef DRUMFREAKE_CORE_PROJECT_H_
#define DRUMFREAKE_CORE_PROJECT_H_

#include <cmath>
#include <cstdint>
#include <string>

#include "core/params.h"

namespace drumfreake {

const int kMaxSteps = 64;
const int kStepsPerPage = 16;
const int kNumPages = kMaxSteps / kStepsPerPage;
const int kNumPatterns = 16;
const int kMaxRatchets = 4;
const int kMaxSongLength = 64;

struct Step {
  bool on;
  float velocity;     // 0 to 1.
  float probability;  // 0 to 1.
  int ratchets;       // 1 to kMaxRatchets.
  int note;           // Semitones relative to the track pitch.
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
  // Polyrhythm: tracks with their own length keep cycling across pattern
  // loops instead of restarting with the pattern.
  bool poly;

  void Clear();
  bool IsEmpty() const;
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
  float bpm;
  float volume;
  // "Chords" mode: each of pads 1-8 plays a set of tracks (bit mask).
  uint8_t chords[8];
  // Keys pad mode: root (0 = C), scale and octave shift, for the whole project.
  int key_root;
  int key_scale;
  int key_octave;
  // MIDI input. Each track listens on its own channel (1-16, 0 = none) and
  // plays the notes it receives at their pitch. On the other channels, the
  // eight notes of a major scale from `midi_trigger_note` trigger tracks A-H
  // at their own pitch. `midi_clock_sync` follows an external clock.
  bool midi_clock_sync;
  int midi_channels[kNumTracks];
  int midi_trigger_note;
  // Song: chain of pattern indices, played in order (a pattern may repeat).
  int song[kMaxSongLength];
  int song_length;

  // Default kit and demo pattern.
  void Init();
};

// Builds the demo groove in `pattern`.
void MakeDemoPattern(Pattern* pattern);

}  // namespace drumfreake

#endif  // DRUMFREAKE_CORE_PROJECT_H_
