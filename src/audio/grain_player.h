// The granular engines: short windowed grains read from a sample of the pool,
// in place of the Plaits voice of a track. Up to kMaxGrains overlap; their
// timing, position, size and pitch drift with Chaos (a per-voice random
// sequence, so the result is repeatable).
//
// Knobs of the Sound page:
//            Harmonics        Timbre    Morph            Colour   Decay    Aux
// Scrub      scan speed (*)   density   chaos            size     release  slot
// Spray      position         density   chaos            size     release  slot
// Burst      position         density   shape (**)       chaos    release  slot
//
// (*) 0 freezes, the middle scans at the sample's own speed, fully up 4 times
//     faster; the scan starts from the beginning on every note and wraps.
// (**) grain size and the length of the burst: short and tight to long.
// Density goes from 2 to 200 grains per second, size from 10 to 500 ms
// (Burst: 5 to 200 ms). The release follows the gate (5 ms to 8 s). Pitch and
// the note set the grains' speed (C4: the sample's own).

#ifndef DRUMFAKE_AUDIO_GRAIN_PLAYER_H_
#define DRUMFAKE_AUDIO_GRAIN_PLAYER_H_

#include <cstdint>

#include "audio/buses.h"
#include "core/sample_file.h"

namespace drumfake {

class GrainPlayer {
 public:
  static const int kMaxGrains = 16;

  void Init(uint32_t seed);
  // Starts over on the next Render (grains already sounding finish).
  void Start();
  // Renders kBlockSize frames of `engine` (kScrubEngine, kSprayEngine or
  // kBurstEngine). See SamplePlayer::Render for the other arguments.
  void Render(const SampleData* sample, int engine, const float* params, float note, bool gate,
              float* out);
  // Making grains, or grains still sounding.
  bool active() const { return pending_ || spawning_ || live_ > 0; }

 private:
  struct Grain {
    double position;  // In the sample, at the grain's start.
    double rate;      // Frames of the sample per output frame.
    float length;     // In output frames.
    float age;
    float gain;
    bool alive;
  };

  float Random();  // [0, 1)
  void Spawn(int engine, int n, double position, double rate, float size, float chaos,
             float gain);

  Grain grains_[kMaxGrains];
  int live_;
  uint32_t rng_;
  bool pending_;
  bool spawning_;
  double scan_;       // Scrub: where the next grain starts.
  double countdown_;  // Frames until the next grain.
  float envelope_;    // Release after the gate.
  float burst_;       // Burst: level of the grains being made, decaying.
};

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_GRAIN_PLAYER_H_
