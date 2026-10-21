#include "core/sample_file.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <numeric>

namespace drumfake {

namespace {

const double kPi = 3.14159265358979323846;

uint16_t Read16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t Read32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
      (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// One sample of a channel, in [-1, 1].
float DecodeSample(const uint8_t* p, int bits, bool is_float) {
  if (is_float) {
    if (bits == 32) {
      uint32_t u = Read32(p);
      float f;
      static_assert(sizeof(f) == sizeof(u), "32-bit float");
      memcpy(&f, &u, sizeof(f));
      return std::isfinite(f) ? f : 0.0f;
    }
    uint64_t u = static_cast<uint64_t>(Read32(p)) | (static_cast<uint64_t>(Read32(p + 4)) << 32);
    double d;
    static_assert(sizeof(d) == sizeof(u), "64-bit float");
    memcpy(&d, &u, sizeof(d));
    return std::isfinite(d) ? static_cast<float>(d) : 0.0f;
  }
  switch (bits) {
    case 8:
      return (static_cast<float>(p[0]) - 128.0f) / 128.0f;  // Unsigned.
    case 16:
      return static_cast<float>(static_cast<int16_t>(Read16(p))) / 32768.0f;
    case 24: {
      int32_t v = static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 8) |
                                       (static_cast<uint32_t>(p[1]) << 16) |
                                       (static_cast<uint32_t>(p[2]) << 24)) >> 8;
      return static_cast<float>(v) / 8388608.0f;
    }
    default:
      return static_cast<float>(static_cast<int32_t>(Read32(p))) / 2147483648.0f;
  }
}

// Windowed-sinc resampling (Blackman window). The low-pass follows the lower
// of the two Nyquist frequencies, so downsampling does not alias. Stops after
// `max_frames` output frames.
std::vector<float> Resample(const std::vector<float>& in, int in_rate, int out_rate,
                            size_t max_frames) {
  if (in_rate == out_rate) {
    return std::vector<float>(in.begin(), in.begin() + std::min(in.size(), max_frames));
  }
  const int g = std::gcd(in_rate, out_rate);
  const int64_t up = out_rate / g;    // Output frames per period.
  const int64_t down = in_rate / g;   // Input frames per period.
  const double scale = std::min(1.0, static_cast<double>(out_rate) / in_rate);
  const double cutoff = 0.97 * scale;  // In units of the input Nyquist frequency.
  const int half = std::min(64, static_cast<int>(std::ceil(16.0 / scale)));
  const int taps = 2 * half;

  auto kernel = [&](int64_t phase, float* weights) {
    double frac = static_cast<double>(phase) / up;
    double sum = 0.0;
    for (int k = 0; k < taps; ++k) {
      double x = (k - half + 1) - frac;  // Distance from the output position.
      double s = std::fabs(x) < 1e-9 ? 1.0 : std::sin(kPi * cutoff * x) / (kPi * cutoff * x);
      double w = (x + half) / taps;      // 0..1 across the window.
      double window = 0.42 - 0.5 * std::cos(2.0 * kPi * w) + 0.08 * std::cos(4.0 * kPi * w);
      weights[k] = static_cast<float>(s * window);
      sum += weights[k];
    }
    for (int k = 0; k < taps; ++k) {
      weights[k] = static_cast<float>(weights[k] / sum);  // Unity gain at DC.
    }
  };

  // Rational rates (44.1 to 48 kHz: 160 phases) get a table of kernels.
  const bool table = up <= 4096;
  std::vector<float> kernels(table ? static_cast<size_t>(up) * taps : taps);
  if (table) {
    for (int64_t p = 0; p < up; ++p) {
      kernel(p, &kernels[static_cast<size_t>(p) * taps]);
    }
  }

  const size_t frames = std::min(
      max_frames, static_cast<size_t>((static_cast<int64_t>(in.size()) * up) / down));
  std::vector<float> out(frames);
  const int64_t size = static_cast<int64_t>(in.size());
  for (size_t n = 0; n < frames; ++n) {
    int64_t position = static_cast<int64_t>(n) * down;
    int64_t i = position / up;
    int64_t phase = position % up;
    const float* weights;
    if (table) {
      weights = &kernels[static_cast<size_t>(phase) * taps];
    } else {
      kernel(phase, kernels.data());
      weights = kernels.data();
    }
    float acc = 0.0f;
    for (int k = 0; k < taps; ++k) {
      int64_t j = i - half + 1 + k;
      if (j >= 0 && j < size) {
        acc += in[static_cast<size_t>(j)] * weights[k];
      }
    }
    out[n] = acc;
  }
  return out;
}

}  // namespace

bool DecodeWav(const uint8_t* bytes, size_t size, SampleData* sample, std::string* error) {
  if (size < 12 || memcmp(bytes, "RIFF", 4) != 0 || memcmp(bytes + 8, "WAVE", 4) != 0) {
    *error = "not a WAV file";
    return false;
  }
  bool have_format = false;
  int channels = 0;
  int rate = 0;
  int bits = 0;
  bool is_float = false;
  const uint8_t* data = nullptr;
  size_t data_size = 0;
  size_t i = 12;
  while (i + 8 <= size) {
    const uint8_t* chunk = bytes + i;
    size_t chunk_size = Read32(chunk + 4);
    size_t available = size - i - 8;
    if (memcmp(chunk, "fmt ", 4) == 0) {
      if (chunk_size < 16 || available < 16) {
        *error = "damaged format chunk";
        return false;
      }
      int tag = Read16(chunk + 8);
      channels = Read16(chunk + 10);
      rate = static_cast<int>(Read32(chunk + 12));
      bits = Read16(chunk + 22);
      if (tag == 0xfffe && chunk_size >= 40 && available >= 40) {
        tag = Read16(chunk + 32);  // The sub-format GUID starts with the format tag.
      }
      if (tag != 1 && tag != 3) {
        *error = "unsupported WAV encoding (only PCM and float)";
        return false;
      }
      is_float = tag == 3;
      have_format = true;
    } else if (memcmp(chunk, "data", 4) == 0) {
      data = chunk + 8;
      data_size = std::min(chunk_size, available);  // A file cut short keeps what it has.
      break;
    }
    if (chunk_size > available) {
      break;
    }
    i += 8 + chunk_size + (chunk_size & 1);  // Chunks are padded to an even size.
  }
  if (!have_format) {
    *error = "no format chunk";
    return false;
  }
  if (!data) {
    *error = "no audio data";
    return false;
  }
  const bool valid_bits = is_float ? (bits == 32 || bits == 64)
                                   : (bits == 8 || bits == 16 || bits == 24 || bits == 32);
  if (!valid_bits || channels < 1 || channels > 32 || rate < 8000 || rate > 192000) {
    *error = "unsupported WAV format (" + std::to_string(channels) + " channels, " +
        std::to_string(rate) + " Hz, " + std::to_string(bits) + " bits)";
    return false;
  }

  const size_t sample_bytes = static_cast<size_t>(bits / 8);
  const size_t frame_bytes = sample_bytes * static_cast<size_t>(channels);
  const size_t source_frames = data_size / frame_bytes;
  // Decode only what can be kept: a little more than the limit, for the resampler.
  const size_t source_limit = static_cast<size_t>(
      std::ceil(static_cast<double>(kMaxSampleSeconds) * rate)) + 64;
  const size_t decoded = std::min(source_frames, source_limit);
  std::vector<float> mono(decoded);
  for (size_t f = 0; f < decoded; ++f) {
    const uint8_t* frame = data + f * frame_bytes;
    float sum = 0.0f;
    for (int c = 0; c < channels; ++c) {
      sum += DecodeSample(frame + c * sample_bytes, bits, is_float);
    }
    mono[f] = sum / static_cast<float>(channels);
  }

  const size_t max_frames = static_cast<size_t>(kMaxSampleSeconds * kSampleFileRate);
  std::vector<float> resampled = Resample(mono, rate, kSampleFileRate, max_frames);
  const double source_seconds = static_cast<double>(source_frames) / rate;

  sample->frames.resize(resampled.size());
  for (size_t f = 0; f < resampled.size(); ++f) {
    float v = std::clamp(resampled[f], -1.0f, 1.0f) * 32767.0f;
    sample->frames[f] = static_cast<int16_t>(std::lround(v));
  }
  sample->source_rate = rate;
  sample->source_channels = channels;
  sample->source_bits = bits;
  sample->source_float = is_float;
  sample->truncated = source_seconds > kMaxSampleSeconds + 1e-6;
  return true;
}

bool LoadWav(const std::string& path, SampleData* sample, std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    *error = "cannot read " + path;
    return false;
  }
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
  if (!DecodeWav(bytes.data(), bytes.size(), sample, error)) {
    *error = path + ": " + *error;
    return false;
  }
  return true;
}

}  // namespace drumfake
