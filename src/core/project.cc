#include "core/project.h"

#include <algorithm>

#include "core/presets.h"

namespace drumfreake {

const float kDivisionSteps[kNumDivisions] = {
  2.0f, 4.0f / 3.0f, 1.0f, 2.0f / 3.0f, 0.5f
};
const char* const kDivisionNames[kNumDivisions] = {
  "1/8", "1/8T", "1/16", "1/16T", "1/32"
};

void Step::Clear() {
  on = false;
  velocity = 0.8f;
  probability = 1.0f;
  ratchets = 1;
  note = 0;
  micro = 0.0f;
  gate = 0.0f;
  slide = false;
  ClearLocks();
}

void Step::ClearLocks() {
  for (int i = 0; i < kNumLockable; ++i) {
    locks[i] = NAN;
  }
}

bool Step::has_any_lock() const {
  for (int i = 0; i < kNumLockable; ++i) {
    if (has_lock(i)) {
      return true;
    }
  }
  return false;
}

void TrackSequence::Clear() {
  for (int i = 0; i < kMaxSteps; ++i) {
    steps[i].Clear();
  }
  length = 0;
}

int TrackSequence::ClearLocks(int param) {
  int cleared = 0;
  for (int i = 0; i < kMaxSteps; ++i) {
    if (steps[i].has_lock(param)) {
      steps[i].locks[param] = NAN;
      ++cleared;
    }
  }
  return cleared;
}

bool TrackSequence::IsEmpty() const {
  for (int i = 0; i < kMaxSteps; ++i) {
    if (steps[i].on) {
      return false;
    }
  }
  return true;
}

void Pattern::Clear() {
  for (int i = 0; i < kNumTracks; ++i) {
    tracks[i].Clear();
  }
  length = kStepsPerPage;
  swing = 0.5f;
  division = kDefaultDivision;
  poly = false;
}

void Pattern::CopyPages(int source, int destination) {
  if (destination > source) {
    int period = (source + 1) * kStepsPerPage;
    int end = (destination + 1) * kStepsPerPage;
    for (TrackSequence& track : tracks) {
      for (int s = period; s < end; ++s) {
        track.steps[s] = track.steps[s % period];
      }
    }
    length = std::max(length, end);
  } else if (destination < source) {
    for (TrackSequence& track : tracks) {
      for (int s = 0; s < kStepsPerPage; ++s) {
        track.steps[destination * kStepsPerPage + s] = track.steps[source * kStepsPerPage + s];
      }
    }
  }
}

bool Pattern::IsEmpty() const {
  for (int i = 0; i < kNumTracks; ++i) {
    if (!tracks[i].IsEmpty()) {
      return false;
    }
  }
  return true;
}

void TrackSound::Reset() {
  name = "Init";
  for (int i = 0; i < kNumTrackParams; ++i) {
    params[i] = track_param_info(i).default_value;
  }
}

namespace {

void Hit(Pattern* p, int track, int step, float velocity = 0.8f,
         int note = 0, int ratchets = 1, float probability = 1.0f) {
  Step& s = p->tracks[track].steps[step];
  s.on = true;
  s.velocity = velocity;
  s.note = note;
  s.ratchets = ratchets;
  s.probability = probability;
}

}  // namespace

void MakeDemoPattern(Pattern* p) {
  p->Clear();
  p->swing = 0.54f;
  for (int i = 0; i < 16; i += 4) {
    Hit(p, 0, i, i == 0 ? 1.0f : 0.85f);  // Kick.
    Hit(p, 2, i, 0.55f);                  // Closed hat.
    Hit(p, 3, i + 2, 0.7f);               // Open hat.
  }
  Hit(p, 2, 7, 0.35f);
  Hit(p, 2, 15, 0.4f, 0, 2);
  Hit(p, 1, 4, 0.9f);  // Snare.
  Hit(p, 1, 12, 0.9f);
  Hit(p, 4, 4, 0.7f);  // Clap.
  Hit(p, 4, 12, 0.7f);
  Hit(p, 5, 13, 0.6f, 0, 2);  // Tom.
  Hit(p, 5, 15, 0.7f, -5);
  Hit(p, 6, 3, 0.6f, 0, 1, 0.5f);  // Cowbell.
  Hit(p, 6, 10, 0.6f);

  // Bass line, with a parameter lock on the octave jump.
  const int kBassSteps[] = { 2, 5, 8, 10, 14 };
  const int kBassNotes[] = { 0, 0, 12, 3, -2 };
  for (int i = 0; i < 5; ++i) {
    Hit(p, 7, kBassSteps[i], 0.8f, kBassNotes[i]);
  }
  p->tracks[7].steps[8].locks[P_TIMBRE] = 0.65f;
}

void Project::Init() {
  const int kDefaultKit[kNumTracks] = {
    PRESET_KICK_808, PRESET_SNARE_808, PRESET_CLOSED_HAT, PRESET_OPEN_HAT,
    PRESET_CLAP, PRESET_LOW_TOM, PRESET_COWBELL, PRESET_ACID_BASS
  };
  for (int i = 0; i < kNumTracks; ++i) {
    ApplyPreset(kDefaultKit[i], &sounds[i]);
  }
  for (int i = 0; i < kNumPatterns; ++i) {
    patterns[i].Clear();
    std::copy(sounds, sounds + kNumTracks, patterns[i].kit);
  }
  MakeDemoPattern(&patterns[0]);
  for (int i = 0; i < M_LAST; ++i) {
    master[i] = master_param_info(i).default_value;
  }
  for (int i = 0; i < A_LAST; ++i) {
    assign[i] = assign_param_info(i).default_value;
  }
  bpm = 120.0f;
  volume = 0.8f;
  // Default drum "chords": kick + hat, kick + snare, snare + clap...
  const uint8_t kDefaultChords[8] = {
    0x05, 0x03, 0x12, 0x09, 0x21, 0x41, 0x81, 0x1f
  };
  for (int i = 0; i < 8; ++i) {
    chords[i] = kDefaultChords[i];
  }
  for (int i = 0; i < kMaxSongLength; ++i) {
    song[i] = 0;
  }
  song_length = 0;
  midi_clock_sync = false;
  switch_at_end = true;
  for (int i = 0; i < kNumTracks; ++i) {
    midi_channels[i] = i + 1;
  }
  midi_trigger_note = 36;  // C2 (C4 = 60): white keys C2-C3 trigger A-H.
  key_root = 0;
  key_scale = 0;
  key_octave = 0;
  for (int i = 0; i < C_LAST; ++i) {
    clouds[i] = clouds_param_info(i).default_value;
  }
  marbles = MarblesSettings();
  marbles_tracks[0] = 0;  // t1 plays the kick...
  marbles_tracks[1] = 1;  // ...and t3 the snare.
}

}  // namespace drumfreake
