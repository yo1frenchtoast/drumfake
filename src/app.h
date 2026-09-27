// GUI application (SDL3 window, ImGui faceplate, SDL3 audio output).

#ifndef DRUMFREAK_APP_H_
#define DRUMFREAK_APP_H_

#include <string>

#include "audio/engine.h"

namespace drumfreak {

struct AppOptions {
  std::string project_path;
  // Debug: render this many frames, save the window to a PPM file and quit.
  std::string screenshot_path;
  int screenshot_frames = 20;
};

int RunApp(Engine* engine, const AppOptions& options);

}  // namespace drumfreak

#endif  // DRUMFREAK_APP_H_
