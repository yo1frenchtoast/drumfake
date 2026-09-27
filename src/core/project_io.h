// Project save/load, as JSON.

#ifndef DRUMFREAKE_CORE_PROJECT_IO_H_
#define DRUMFREAKE_CORE_PROJECT_IO_H_

#include <string>

#include "core/project.h"

namespace drumfreake {

bool SaveProject(const Project& project, const std::string& path, std::string* error);
bool LoadProject(const std::string& path, Project* project, std::string* error);

// ~/.local/share/drumfreake/project.json (or $XDG_DATA_HOME/drumfreake/...).
std::string DefaultProjectPath();

}  // namespace drumfreake

#endif  // DRUMFREAKE_CORE_PROJECT_IO_H_
