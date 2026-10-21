// Audio files for the sampler: WAV decoding into the format samples are kept
// in (mono, 16-bit, at the engine's sample rate).

#ifndef DRUMFAKE_CORE_SAMPLE_FILE_H_
#define DRUMFAKE_CORE_SAMPLE_FILE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace drumfake {

// Samples are resampled to this rate when loaded (the engine's, see
// audio/buses.h) and cut after kMaxSampleSeconds.
const int kSampleFileRate = 48000;
const float kMaxSampleSeconds = 60.0f;

struct SampleData {
  std::vector<int16_t> frames;  // Mono, kSampleFileRate.
  // What the file held.
  int source_rate = 0;
  int source_channels = 0;
  int source_bits = 0;
  bool source_float = false;
  bool truncated = false;  // Longer than kMaxSampleSeconds: the end was cut.

  float seconds() const { return static_cast<float>(frames.size()) / kSampleFileRate; }
};

// Decodes a WAV file: PCM 8, 16, 24 or 32 bits, or float 32 or 64 bits (also in
// WAVE_FORMAT_EXTENSIBLE), any number of channels (mixed down to mono), any
// rate from 8 to 192 kHz. Unknown chunks are skipped. A file cut short keeps the
// frames it holds.
bool DecodeWav(const uint8_t* bytes, size_t size, SampleData* sample, std::string* error);
bool LoadWav(const std::string& path, SampleData* sample, std::string* error);

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_SAMPLE_FILE_H_
