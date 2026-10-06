#include "core/scales.h"

#include <cstdio>

namespace drumfreake {

namespace {

const Scale kScales[kNumScales] = {
  { "Major", 7, { 0, 2, 4, 5, 7, 9, 11 } },
  { "Minor", 7, { 0, 2, 3, 5, 7, 8, 10 } },
  { "Dorian", 7, { 0, 2, 3, 5, 7, 9, 10 } },
  { "Phrygian", 7, { 0, 1, 3, 5, 7, 8, 10 } },
  { "Lydian", 7, { 0, 2, 4, 6, 7, 9, 11 } },
  { "Mixolydian", 7, { 0, 2, 4, 5, 7, 9, 10 } },
  { "Harm. Minor", 7, { 0, 2, 3, 5, 7, 8, 11 } },
  { "Pent. Major", 5, { 0, 2, 4, 7, 9 } },
  { "Pent. Minor", 5, { 0, 3, 5, 7, 10 } },
  { "Blues", 6, { 0, 3, 5, 6, 7, 10 } },
  // The piano: pads 1-8 are the white keys, the pads A-H the black ones.
  { "Chromatic", 7, { 0, 2, 4, 5, 7, 9, 11 } },
};

const char* const kPitchClasses[12] = {
  "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

}  // namespace

const Scale& scale(int index) {
  return kScales[index < 0 || index >= kNumScales ? 0 : index];
}

int ScaleDegree(int scale_index, int degree) {
  const Scale& s = scale(scale_index);
  return (degree / s.size) * 12 + s.intervals[degree % s.size];
}

int MajorScaleDegree(int root_note, int note) {
  for (int degree = 0; degree < 8; ++degree) {
    if (root_note + ScaleDegree(0, degree) == note) {
      return degree;
    }
  }
  return -1;
}

int KeyOffset(int root, int scale_index, int octave, int pad) {
  return root + ScaleDegree(scale_index, pad) + 12 * octave;
}

bool KeySharpOffset(int root, int scale_index, int octave, int pad, int* offset) {
  int base = KeyOffset(root, scale_index, octave, pad);
  int next = KeyOffset(root, scale_index, octave, pad + 1);
  if (next - base < 2) {
    return false;  // The next note is a semitone up: no key in between.
  }
  *offset = base + 1;
  return true;
}

namespace {

const ChordShape kChordShapes[kNumChordShapes] = {
  { "Triad", 2, { 2, 4 } },
  { "7th", 3, { 2, 4, 6 } },
  { "Sus2", 2, { 1, 4 } },
  { "Sus4", 2, { 3, 4 } },
  { "Add9", 3, { 2, 4, 8 } },
  { "6th", 3, { 2, 4, 5 } },
  { "9th", 4, { 2, 4, 6, 8 } },
  { "Power", 1, { 4 } },
};

struct ChordSuffix {
  const char* suffix;
  int size;
  int intervals[4];
};

const ChordSuffix kChordSuffixes[] = {
  { "", 2, { 4, 7 } },
  { "m", 2, { 3, 7 } },
  { "dim", 2, { 3, 6 } },
  { "aug", 2, { 4, 8 } },
  { "sus2", 2, { 2, 7 } },
  { "sus4", 2, { 5, 7 } },
  { "5", 1, { 7 } },
  { "7", 3, { 4, 7, 10 } },
  { "maj7", 3, { 4, 7, 11 } },
  { "m7", 3, { 3, 7, 10 } },
  { "mMaj7", 3, { 3, 7, 11 } },
  { "m7b5", 3, { 3, 6, 10 } },
  { "dim7", 3, { 3, 6, 9 } },
  { "6", 3, { 4, 7, 9 } },
  { "m6", 3, { 3, 7, 9 } },
  { "add9", 3, { 4, 7, 14 } },
  { "m add9", 3, { 3, 7, 14 } },
  { "9", 4, { 4, 7, 10, 14 } },
  { "maj9", 4, { 4, 7, 11, 14 } },
  { "m9", 4, { 3, 7, 10, 14 } },
};

}  // namespace

const ChordShape& chord_shape(int index) {
  return kChordShapes[index < 0 || index >= kNumChordShapes ? 0 : index];
}

int ChordNotes(int root, int scale_index, int octave, int degree, int shape, int* notes) {
  const ChordShape& s = chord_shape(shape);
  const int base = kKeyBaseNote + root + 12 * octave;
  notes[0] = base + ScaleDegree(scale_index, degree);
  for (int i = 0; i < s.size; ++i) {
    notes[1 + i] = base + ScaleDegree(scale_index, degree + s.steps[i]);
  }
  return 1 + s.size;
}

std::string ChordName(const int* notes, int count) {
  std::string name = PitchClassName(notes[0]);
  for (const ChordSuffix& c : kChordSuffixes) {
    if (c.size != count - 1) {
      continue;
    }
    bool same = true;
    for (int i = 0; i < c.size && same; ++i) {
      same = notes[1 + i] - notes[0] == c.intervals[i];
    }
    if (same) {
      return name + c.suffix;
    }
  }
  return name;
}

const char* PitchClassName(int pitch_class) {
  return kPitchClasses[((pitch_class % 12) + 12) % 12];
}

std::string NoteName(int midi_note) {
  char buffer[16];
  int octave = (midi_note >= 0 ? midi_note / 12 : (midi_note - 11) / 12) - 1;
  snprintf(buffer, sizeof(buffer), "%s%d", PitchClassName(midi_note), octave);
  return buffer;
}

}  // namespace drumfreake
