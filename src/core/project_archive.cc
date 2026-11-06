#include "core/project_archive.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>

#include "core/project_io.h"
#include "core/sample_pool.h"

namespace drumfake {

namespace {

const char kProjectEntry[] = "project.json";
const char kSampleFolder[] = "samples/";

const uint32_t kLocalHeader = 0x04034b50;
const uint32_t kCentralHeader = 0x02014b50;
const uint32_t kEndOfCentral = 0x06054b50;
const uint16_t kUtf8Names = 0x0800;  // General purpose flag: names are UTF-8.

void Put16(std::string* out, uint32_t value) {
  out->push_back(static_cast<char>(value & 0xff));
  out->push_back(static_cast<char>((value >> 8) & 0xff));
}

void Put32(std::string* out, uint32_t value) {
  Put16(out, value & 0xffff);
  Put16(out, value >> 16);
}

uint32_t Get16(const std::string& in, size_t at) {
  return static_cast<uint8_t>(in[at]) | (static_cast<uint8_t>(in[at + 1]) << 8);
}

uint32_t Get32(const std::string& in, size_t at) {
  return Get16(in, at) | (Get16(in, at + 2) << 16);
}

// The time of writing, in MS-DOS format.
void DosTime(uint16_t* time, uint16_t* date) {
  std::time_t now = std::time(nullptr);
  std::tm local = *std::localtime(&now);
  *time = static_cast<uint16_t>((local.tm_hour << 11) | (local.tm_min << 5) | (local.tm_sec / 2));
  *date = static_cast<uint16_t>(((std::max(local.tm_year, 80) - 80) << 9) |
                                ((local.tm_mon + 1) << 5) | local.tm_mday);
}

bool ReadFile(const std::filesystem::path& path, std::string* data) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  data->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return !in.bad();
}

}  // namespace

uint32_t Crc32(const std::string& data) {
  static uint32_t table[256];
  static bool ready = false;
  if (!ready) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
      }
      table[i] = c;
    }
    ready = true;
  }
  uint32_t crc = 0xffffffffu;
  for (char byte : data) {
    crc = table[(crc ^ static_cast<uint8_t>(byte)) & 0xff] ^ (crc >> 8);
  }
  return crc ^ 0xffffffffu;
}

std::string MakeZip(const std::vector<ZipEntry>& entries) {
  uint16_t time;
  uint16_t date;
  DosTime(&time, &date);
  std::string out;
  std::string central;
  for (const ZipEntry& entry : entries) {
    uint32_t crc = Crc32(entry.data);
    uint32_t size = static_cast<uint32_t>(entry.data.size());
    uint32_t offset = static_cast<uint32_t>(out.size());
    Put32(&out, kLocalHeader);
    Put16(&out, 20);  // Version needed: 2.0.
    Put16(&out, kUtf8Names);
    Put16(&out, 0);  // Stored.
    Put16(&out, time);
    Put16(&out, date);
    Put32(&out, crc);
    Put32(&out, size);
    Put32(&out, size);
    Put16(&out, static_cast<uint32_t>(entry.name.size()));
    Put16(&out, 0);  // No extra field.
    out += entry.name;
    out += entry.data;

    Put32(&central, kCentralHeader);
    Put16(&central, 20);  // Made by: 2.0.
    Put16(&central, 20);
    Put16(&central, kUtf8Names);
    Put16(&central, 0);
    Put16(&central, time);
    Put16(&central, date);
    Put32(&central, crc);
    Put32(&central, size);
    Put32(&central, size);
    Put16(&central, static_cast<uint32_t>(entry.name.size()));
    Put16(&central, 0);  // Extra field.
    Put16(&central, 0);  // Comment.
    Put16(&central, 0);  // Disk.
    Put16(&central, 0);  // Internal attributes.
    Put32(&central, 0);  // External attributes.
    Put32(&central, offset);
    central += entry.name;
  }
  uint32_t central_offset = static_cast<uint32_t>(out.size());
  out += central;
  Put32(&out, kEndOfCentral);
  Put16(&out, 0);
  Put16(&out, 0);
  Put16(&out, static_cast<uint32_t>(entries.size()));
  Put16(&out, static_cast<uint32_t>(entries.size()));
  Put32(&out, static_cast<uint32_t>(central.size()));
  Put32(&out, central_offset);
  Put16(&out, 0);  // Comment.
  return out;
}

bool ReadZip(const std::string& zip, std::vector<ZipEntry>* entries, std::string* error) {
  entries->clear();
  // The end of central directory record: 22 bytes and a comment of up to 64 KB.
  size_t end = std::string::npos;
  if (zip.size() >= 22) {
    size_t lowest = zip.size() > 22 + 0xffff ? zip.size() - 22 - 0xffff : 0;
    for (size_t at = zip.size() - 22; ; --at) {
      if (Get32(zip, at) == kEndOfCentral) {
        end = at;
        break;
      }
      if (at == lowest) {
        break;
      }
    }
  }
  if (end == std::string::npos) {
    *error = "not a zip file";
    return false;
  }
  uint32_t count = Get16(zip, end + 10);
  size_t at = Get32(zip, end + 16);
  for (uint32_t i = 0; i < count; ++i) {
    if (at + 46 > zip.size() || Get32(zip, at) != kCentralHeader) {
      *error = "damaged zip file";
      return false;
    }
    uint32_t method = Get16(zip, at + 10);
    uint32_t crc = Get32(zip, at + 16);
    uint32_t compressed = Get32(zip, at + 20);
    uint32_t size = Get32(zip, at + 24);
    size_t name_length = Get16(zip, at + 28);
    size_t extra_length = Get16(zip, at + 30);
    size_t comment_length = Get16(zip, at + 32);
    size_t local = Get32(zip, at + 42);
    if (at + 46 + name_length > zip.size()) {
      *error = "damaged zip file";
      return false;
    }
    ZipEntry entry;
    entry.name = zip.substr(at + 46, name_length);
    at += 46 + name_length + extra_length + comment_length;
    if (!entry.name.empty() && entry.name.back() == '/') {
      continue;  // A folder.
    }
    if (method != 0 || compressed != size) {
      *error = entry.name + " is compressed: only archives exported by Drumfake can be read";
      return false;
    }
    if (local + 30 > zip.size() || Get32(zip, local) != kLocalHeader) {
      *error = "damaged zip file";
      return false;
    }
    size_t data = local + 30 + Get16(zip, local + 26) + Get16(zip, local + 28);
    if (data + size > zip.size()) {
      *error = "damaged zip file";
      return false;
    }
    entry.data = zip.substr(data, size);
    if (Crc32(entry.data) != crc) {
      *error = entry.name + " is damaged";
      return false;
    }
    entries->push_back(std::move(entry));
  }
  return true;
}

std::string WriteProjectArchive(const Project& project, const std::string& sample_directory,
                                std::vector<std::string>* problems) {
  std::vector<ZipEntry> entries;
  entries.push_back({ kProjectEntry, ProjectToJson(project) });
  for (int slot = 0; slot < kNumSampleSlots; ++slot) {
    const std::string& name = project.samples[slot];
    if (name.empty() || FindSampleSlot(project, name) != slot) {
      continue;  // Empty, or already in the archive.
    }
    ZipEntry entry;
    entry.name = kSampleFolder + name;
    if (ReadFile(std::filesystem::path(sample_directory) / name, &entry.data)) {
      entries.push_back(std::move(entry));
    } else {
      problems->push_back("missing sample: " + name);
    }
  }
  return MakeZip(entries);
}

bool ReadProjectArchive(const std::string& zip, const std::string& sample_directory,
                        Project* project, std::vector<std::string>* problems,
                        std::string* error) {
  std::vector<ZipEntry> entries;
  if (!ReadZip(zip, &entries, error)) {
    return false;
  }
  const ZipEntry* project_entry = nullptr;
  for (const ZipEntry& entry : entries) {
    if (entry.name == kProjectEntry) {
      project_entry = &entry;
    }
  }
  if (!project_entry) {
    *error = "no project.json in the archive";
    return false;
  }
  auto loaded = std::make_unique<Project>();
  if (!ProjectFromJson(project_entry->data, loaded.get(), error)) {
    return false;
  }
  // Sample names come from ProjectFromJson, which only keeps plain file names.
  const auto original = std::make_unique<Project>(*loaded);
  for (int slot = 0; slot < kNumSampleSlots; ++slot) {
    const std::string& name = original->samples[slot];
    if (name.empty() || FindSampleSlot(*original, name) != slot) {
      continue;  // Empty, or done with an earlier slot.
    }
    const ZipEntry* sample = nullptr;
    for (const ZipEntry& entry : entries) {
      if (entry.name == kSampleFolder + name) {
        sample = &entry;
      }
    }
    if (!sample) {
      problems->push_back("not in the archive: " + name);
      continue;
    }
    std::string written;
    std::string write_error;
    if (!WriteIntoSampleDirectory(name, sample->data, sample_directory, &written,
                                  &write_error)) {
      *error = write_error;
      return false;
    }
    // Every slot naming this file follows the new name.
    for (int other = slot; other < kNumSampleSlots; ++other) {
      if (original->samples[other] == name) {
        loaded->samples[other] = written;
      }
    }
  }
  *project = *loaded;
  return true;
}

std::string ArchiveFileName() {
  std::time_t now = std::time(nullptr);
  char name[64];
  std::strftime(name, sizeof(name), "drumfake-%Y-%m-%d-%H%M.zip", std::localtime(&now));
  return name;
}

}  // namespace drumfake
