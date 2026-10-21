#include "core/settings.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace drumfake {

std::string SettingsPath(const std::string& project_path) {
  return (std::filesystem::path(project_path).parent_path() / "settings.json").string();
}

bool LoadSettings(const std::string& path, Settings* settings) {
  std::ifstream in(path);
  if (!in) {
    return false;
  }
  try {
    nlohmann::json j = nlohmann::json::parse(in);
    if (j.contains("theme") && j["theme"].is_string()) {
      settings->theme = j["theme"].get<std::string>();
    }
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool SaveSettings(const std::string& path, const Settings& settings, std::string* error) {
  std::error_code ec;
  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path(), ec);
  }
  std::ofstream out(path);
  if (!out) {
    *error = "cannot write " + path;
    return false;
  }
  out << nlohmann::json{ { "theme", settings.theme } }.dump(1);
  return static_cast<bool>(out);
}

}  // namespace drumfake
