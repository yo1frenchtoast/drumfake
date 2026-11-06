#include "core/sample_pool.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace drumfake {

namespace fs = std::filesystem;

namespace {

bool SameContent(const std::string& data, const fs::path& file) {
  std::error_code ec;
  if (fs::file_size(file, ec) != data.size() || ec) {
    return false;
  }
  std::ifstream in(file, std::ios::binary);
  return std::equal(data.begin(), data.end(), std::istreambuf_iterator<char>(in));
}

}  // namespace

std::string SampleDirectory(const std::string& project_path) {
  return (fs::path(project_path).parent_path() / "samples").string();
}

bool CopyIntoSampleDirectory(const std::string& source, const std::string& directory,
                             std::string* file_name, std::string* error) {
  std::error_code ec;
  fs::path from(source);
  std::ifstream in(from, std::ios::binary);
  if (!fs::is_regular_file(from, ec) || !in) {
    *error = "cannot read " + source;
    return false;
  }
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return WriteIntoSampleDirectory(from.filename().string(), data, directory, file_name, error);
}

bool WriteIntoSampleDirectory(const std::string& name, const std::string& data,
                              const std::string& directory, std::string* file_name,
                              std::string* error) {
  std::error_code ec;
  fs::create_directories(directory, ec);
  fs::path from(name);
  std::string stem = from.stem().string();
  std::string extension = from.extension().string();
  if (stem.empty()) {
    stem = "sample";
  }
  for (int n = 1; n < 1000; ++n) {
    std::string candidate = stem + (n > 1 ? " " + std::to_string(n) : "") + extension;
    fs::path target = fs::path(directory) / candidate;
    if (fs::exists(target, ec)) {
      if (SameContent(data, target)) {
        *file_name = candidate;  // Already there.
        return true;
      }
      continue;
    }
    std::ofstream out(target, std::ios::binary);
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!out) {
      *error = "cannot write " + candidate + " in " + directory;
      return false;
    }
    *file_name = candidate;
    return true;
  }
  *error = "too many files named " + stem + extension;
  return false;
}

int FindSampleSlot(const Project& project, const std::string& file) {
  for (int i = 0; i < kNumSampleSlots; ++i) {
    if (!file.empty() && project.samples[i] == file) {
      return i;
    }
  }
  return -1;
}

int FirstFreeSampleSlot(const Project& project) {
  for (int i = 0; i < kNumSampleSlots; ++i) {
    if (project.samples[i].empty()) {
      return i;
    }
  }
  return -1;
}

void LoadSamplePool(const Project& project, const std::string& directory, SamplePoolLoad* load) {
  for (int i = 0; i < kNumSampleSlots; ++i) {
    load->slots[i].reset();
    if (project.samples[i].empty()) {
      continue;
    }
    auto sample = std::make_shared<SampleData>();
    std::string error;
    if (!LoadWav((fs::path(directory) / project.samples[i]).string(), sample.get(), &error)) {
      load->problems.push_back("slot " + std::to_string(i + 1) + ": " + error);
      continue;
    }
    load->bytes += sample->frames.size() * sizeof(int16_t);
    load->slots[i] = std::move(sample);
  }
}

}  // namespace drumfake
