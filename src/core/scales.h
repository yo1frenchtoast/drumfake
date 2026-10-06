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
const int kChromaticScale = kNumScales - 1;  // The default: a piano.
const int kMinKeyOctave = -3;
const int kMaxKeyOctave = 3;

const Scale& scale(int index);

// Semitones from the root of the `degree`-th note of a scale (degree 0 is the
// root; degrees past the octave continue in the next octaves).
int ScaleDegree(int scale_index, int degree);

// Degree (0-7) of `note` in the major scale starting at `root_note`, or -1
// when the note is not one of its first eight notes.
int MajorScaleDegree(int root_note, int note);

// Offset of pad `pad` (0-7) in Keys mode, relative to the track pitch.
int KeyOffset(int root, int scale_index, int octave, int pad);

// The key between pad `pad` and the next one, a semitone above it, as on the black
// keys of a piano (pads A-H in Keys mode). False when the next pad is a semitone
// away (E-F, B-C) and there is no such key.
bool KeySharpOffset(int root, int scale_index, int octave, int pad, int* offset);

// "C", "C#"... for a pitch class, "C3"-style names for a MIDI note.
const char* PitchClassName(int pitch_class);
std::string NoteName(int midi_note);

}  // namespace drumfreake

#endif  // DRUMFREAKE_CORE_SCALES_H_
