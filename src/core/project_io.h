// Project save/load, as JSON.

#ifndef DRUMFREAK_CORE_PROJECT_IO_H_
#define DRUMFREAK_CORE_PROJECT_IO_H_

#include <string>

#include "core/project.h"

namespace drumfreak {

bool SaveProject(const Project& project, const std::string& path, std::string* error);
bool LoadProject(const std::string& path, Project* project, std::string* error);

// ~/.local/share/drumfreak/project.json (or $XDG_DATA_HOME/drumfreak/...).
std::string DefaultProjectPath();

}  // namespace drumfreak

#endif  // DRUMFREAK_CORE_PROJECT_IO_H_
