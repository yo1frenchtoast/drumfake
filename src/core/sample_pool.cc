#include "core/sample_pool.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace drumfake {

namespace fs = std::filesystem;

namespace {

bool SameContent(const fs::path& a, const fs::path& b) {
  std::error_code ec;
  if (fs::file_size(a, ec) != fs::file_size(b, ec) || ec) {
    return false;
  }
  std::ifstream fa(a, std::ios::binary);
  std::ifstream fb(b, std::ios::binary);
  return std::equal(std::istreambuf_iterator<char>(fa), std::istreambuf_iterator<char>(),
                    std::istreambuf_iterator<char>(fb));
}

}  // namespace

std::string SampleDirectory(const std::string& project_path) {
  return (fs::path(project_path).parent_path() / "samples").string();
}

bool CopyIntoSampleDirectory(const std::string& source, const std::string& directory,
                             std::string* file_name, std::string* error) {
  std::error_code ec;
  fs::path from(source);
  if (!fs::is_regular_file(from, ec)) {
    *error = "cannot read " + source;
    return false;
  }
  fs::create_directories(directory, ec);
  std::string stem = from.stem().string();
  std::string extension = from.extension().string();
  if (stem.empty()) {
    stem = "sample";
  }
  for (int n = 1; n < 1000; ++n) {
    std::string name = stem + (n > 1 ? " " + std::to_string(n) : "") + extension;
    fs::path target = fs::path(directory) / name;
    if (fs::exists(target, ec)) {
      if (SameContent(from, target)) {
        *file_name = name;  // Already there.
        return true;
      }
      continue;
    }
    if (!fs::copy_file(from, target, ec) || ec) {
      *error = "cannot copy " + source + " to " + directory;
      return false;
    }
    *file_name = name;
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
