// Offline rendering to a WAV file (headless mode and tests).

#ifndef DRUMFAKE_AUDIO_WAV_RENDER_H_
#define DRUMFAKE_AUDIO_WAV_RENDER_H_

#include <string>
#include <vector>

#include "audio/engine.h"

namespace drumfake {

struct RenderStats {
  float peak = 0.0f;
  float rms = 0.0f;
  int non_finite = 0;
  float track_peak[kNumTracks] = {};
  double seconds = 0.0;
  double render_seconds = 0.0;  // Wall time spent rendering.
};

// Plays the current pattern of `engine` for `bars` bars (16 steps each) plus
// a one-second tail. Writes a 16-bit stereo WAV when `path` is not empty.
bool RenderBars(Engine* engine, int bars, const std::string& path,
                RenderStats* stats, std::string* error);

bool WriteWav(const std::string& path, const std::vector<float>& interleaved,
              int channels, int sample_rate);

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_WAV_RENDER_H_
