// Project save/load, as JSON.

#ifndef DRUMFAKE_CORE_PROJECT_IO_H_
#define DRUMFAKE_CORE_PROJECT_IO_H_

#include <string>
#include <vector>

#include "core/project.h"

namespace drumfake {

bool SaveProject(const Project& project, const std::string& path, std::string* error);
bool LoadProject(const std::string& path, Project* project, std::string* error);

// Kit library: one JSON file per kit (the 8 sounds of a pattern), in a
// directory shared by all projects.
struct KitFile {
  std::string path;
  std::string name;     // "Kit 3".
  std::string summary;  // Names of the first sounds, to tell kits apart.
};

// Directory of the kit library for a project file: "kits" beside it.
std::string KitDirectory(const std::string& project_path);
// Kits of `directory`, in creation order. A missing directory gives none.
std::vector<KitFile> ListKits(const std::string& directory);
// Writes `kit` as the next free "Kit N" (or over `path` if it is not empty).
bool SaveKitFile(const std::string& directory, const TrackSound* kit,
                 const std::string& path, std::string* error);
bool LoadKitFile(const std::string& path, TrackSound* kit, std::string* error);
bool DeleteKitFile(const std::string& path, std::string* error);

// ~/.local/share/drumfake/project.json (or $XDG_DATA_HOME/drumfake/...).
std::string DefaultProjectPath();

// The project was named "drumfreake" before: if the directory of `project_path`
// does not exist yet, renames the old "drumfreake" directory beside it (project
// and kit library). Returns true if it moved something.
bool MigrateLegacyDataDirectory(const std::string& project_path);

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_PROJECT_IO_H_
