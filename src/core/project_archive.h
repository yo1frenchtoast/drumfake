// Project archives: a project and the samples of its pool in one .zip file,
// to move a project to another machine or keep a copy of it.
//
// The archive holds "project.json" (the project file) and "samples/<name>"
// for each file of the pool. Entries are stored without compression (WAV files
// hardly compress), and only stored entries can be read back.

#ifndef DRUMFAKE_CORE_PROJECT_ARCHIVE_H_
#define DRUMFAKE_CORE_PROJECT_ARCHIVE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "core/project.h"

namespace drumfake {

struct ZipEntry {
  std::string name;
  std::string data;
};

uint32_t Crc32(const std::string& data);
// A .zip file holding `entries`, stored.
std::string MakeZip(const std::vector<ZipEntry>& entries);
// The entries of a .zip file. Fails on a damaged file and on compressed entries.
bool ReadZip(const std::string& zip, std::vector<ZipEntry>* entries, std::string* error);

// The archive of `project`, with the files of its pool read from
// `sample_directory`. A file that cannot be read is left out, with a line in
// `problems`: its slot keeps the name.
std::string WriteProjectArchive(const Project& project, const std::string& sample_directory,
                                std::vector<std::string>* problems);

// Reads an archive into `project` and writes its samples into
// `sample_directory`. A sample whose name is taken there by another file is
// written as "<name> 2" (and the pool follows); a sample missing from the
// archive gets a line in `problems`. `project` is left as it is on failure.
bool ReadProjectArchive(const std::string& zip, const std::string& sample_directory,
                        Project* project, std::vector<std::string>* problems,
                        std::string* error);

// Default name of an exported archive, e.g. "drumfake-2026-11-06-1432.zip".
std::string ArchiveFileName();

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_PROJECT_ARCHIVE_H_
