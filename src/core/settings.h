// Settings of the machine, not of a project: they stay when another project
// is opened. Saved as settings.json beside the project file.

#ifndef DRUMFAKE_CORE_SETTINGS_H_
#define DRUMFAKE_CORE_SETTINGS_H_

#include <string>

namespace drumfake {

struct Settings {
  std::string theme;  // A theme id (ui/theme.h); empty = the default one.
};

std::string SettingsPath(const std::string& project_path);
// A missing or unreadable file leaves `settings` as it is.
bool LoadSettings(const std::string& path, Settings* settings);
bool SaveSettings(const std::string& path, const Settings& settings, std::string* error);

}  // namespace drumfake

#endif  // DRUMFAKE_CORE_SETTINGS_H_
