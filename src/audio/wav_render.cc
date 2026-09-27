#include "audio/wav_render.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace drumfreak {

namespace {

void Write16(FILE* f, uint16_t v) {
  uint8_t b[2] = { static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8) };
  fwrite(b, 1, 2, f);
}

void Write32(FILE* f, uint32_t v) {
  uint8_t b[4] = {
    static_cast<uint8_t>(v), static_cast<uint8_t>(v >> 8),
    static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 24)
  };
  fwrite(b, 1, 4, f);
}

}  // namespace

bool WriteWav(const std::string& path, const std::vector<float>& interleaved,
              int channels, int sample_rate) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) {
    return false;
  }
  uint32_t data_size = static_cast<uint32_t>(interleaved.size() * 2);
  fwrite("RIFF", 1, 4, f);
  Write32(f, 36 + data_size);
  fwrite("WAVEfmt ", 1, 8, f);
  Write32(f, 16);
  Write16(f, 1);  // PCM.
  Write16(f, static_cast<uint16_t>(channels));
  Write32(f, static_cast<uint32_t>(sample_rate));
  Write32(f, static_cast<uint32_t>(sample_rate * channels * 2));
  Write16(f, static_cast<uint16_t>(channels * 2));
  Write16(f, 16);
  fwrite("data", 1, 4, f);
  Write32(f, data_size);
  for (float s : interleaved) {
    float clipped = std::max(-1.0f, std::min(1.0f, std::isfinite(s) ? s : 0.0f));
    Write16(f, static_cast<uint16_t>(static_cast<int16_t>(clipped * 32767.0f)));
  }
  bool ok = ferror(f) == 0;
  fclose(f);
  return ok;
}

bool RenderBars(Engine* engine, int bars, const std::string& path,
                RenderStats* stats, std::string* error) {
  std::vector<float> audio;
  size_t chunk = 256;
  std::vector<float> buffer(chunk * 2);
  {
    std::lock_guard<std::mutex> lock(engine->mutex());
    engine->Play();
  }
  double bpm = engine->project().bpm;
  double seconds = bars * 16 * 15.0 / bpm;
  size_t play_frames = static_cast<size_t>(seconds * kSampleRate);
  size_t total_frames = play_frames + static_cast<size_t>(kSampleRate);

  auto t0 = std::chrono::steady_clock::now();
  double sum_squares = 0.0;
  for (size_t done = 0; done < total_frames; done += chunk) {
    if (done >= play_frames) {
      std::lock_guard<std::mutex> lock(engine->mutex());
      if (engine->playing()) {
        engine->Stop();
      }
    }
    engine->Render(buffer.data(), chunk);
    for (int t = 0; t < kNumTracks; ++t) {
      stats->track_peak[t] = std::max(stats->track_peak[t],
                                      engine->track_peak[t].load());
    }
    for (float s : buffer) {
      if (!std::isfinite(s)) {
        ++stats->non_finite;
        continue;
      }
      stats->peak = std::max(stats->peak, std::fabs(s));
      sum_squares += static_cast<double>(s) * s;
    }
    audio.insert(audio.end(), buffer.begin(), buffer.end());
  }
  stats->render_seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - t0).count();
  stats->seconds = static_cast<double>(audio.size() / 2) / kSampleRate;
  stats->rms = static_cast<float>(std::sqrt(sum_squares / std::max<size_t>(1, audio.size())));

  if (!path.empty() && !WriteWav(path, audio, 2, static_cast<int>(kSampleRate))) {
    *error = "cannot write " + path;
    return false;
  }
  return true;
}

}  // namespace drumfreak
