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
// MIDI note of the first pad with root C and octave 0: a standard keyboard starting on C3.
const int kKeyBaseNote = 48;
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

// Chords of the Chords pad mode, built on the notes of the scale: the root is a degree of
// the scale and the other notes are higher degrees (a triad is the degrees 0, 2 and 4).
const int kMaxChordNotes = 5;
const int kNumChordShapes = 8;
struct ChordShape {
  const char* name;
  int size;      // Notes besides the root.
  int steps[4];  // Their degrees above the root.
};
const ChordShape& chord_shape(int index);

// MIDI notes (the root first) of the chord of `shape` built on degree `degree` of the
// scale, on the keyboard of the Keys mode. Returns the number of notes.
int ChordNotes(int root, int scale_index, int octave, int degree, int shape, int* notes);

// "Cm7", "G", "Bdim"... for chord notes (the root first); just the root's name when the
// intervals have no usual name.
std::string ChordName(const int* notes, int count);

// "C", "C#"... for a pitch class, "C3"-style names for a MIDI note.
const char* PitchClassName(int pitch_class);
std::string NoteName(int midi_note);

}  // namespace drumfreake

#endif  // DRUMFREAKE_CORE_SCALES_H_
