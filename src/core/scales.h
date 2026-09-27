// Scales for the Keys pad mode.

#ifndef DRUMFREAKE_CORE_SCALES_H_
#define DRUMFREAKE_CORE_SCALES_H_

#include <string>

namespace drumfreake {

struct Scale {
  const char* name;
  int size;           // Notes per octave.
  int intervals[12];  // Semitones from the root.
};

const int kNumScales = 11;
const int kMinKeyOctave = -3;
const int kMaxKeyOctave = 3;

const Scale& scale(int index);

// Semitones from the root of the `degree`-th note of a scale (degree 0 is the
// root; degrees past the octave continue in the next octaves).
int ScaleDegree(int scale_index, int degree);

// Offset of pad `pad` (0-7) in Keys mode, relative to the track pitch.
int KeyOffset(int root, int scale_index, int octave, int pad);

// "C", "C#"... for a pitch class, "C3"-style names for a MIDI note.
const char* PitchClassName(int pitch_class);
std::string NoteName(int midi_note);

}  // namespace drumfreake

#endif  // DRUMFREAKE_CORE_SCALES_H_
