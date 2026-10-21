// GUI application (SDL3 window, ImGui faceplate, SDL3 audio output).

#ifndef DRUMFAKE_APP_H_
#define DRUMFAKE_APP_H_

#include <string>

#include "audio/engine.h"

namespace drumfake {

struct AppOptions {
  std::string project_path;
  // Debug: render this many frames, save the window to a PPM file and quit.
  std::string screenshot_path;
  // A theme id ("cream", "classic") for this run, over the saved setting.
  std::string theme;
  int screenshot_frames = 20;
};

int RunApp(Engine* engine, const AppOptions& options);

}  // namespace drumfake

#endif  // DRUMFAKE_APP_H_
