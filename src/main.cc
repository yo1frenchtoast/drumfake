// Drumfake simulator: a groovebox built on Mutable Instruments Plaits.
//
// Usage:
//   drumfake [--project FILE]
//   drumfake --render OUT.wav [--bars N] [--project FILE] [--pattern N] [--grids]

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#ifndef DRUMFAKE_HEADLESS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // Renames main() to SDL_main() where needed (Android).
#endif

#include "app.h"
#include "audio/engine.h"
#include "audio/sample_loader.h"
#include "audio/wav_render.h"
#include "core/project_io.h"

using namespace drumfake;

namespace {

void PrintUsage() {
  printf("usage: drumfake [--project FILE] [--midi-monitor] [--theme cream|classic]\n"
         "       drumfake --render OUT.wav [--bars N] [--project FILE]"
         " [--pattern N] [--grids]\n");
}

}  // namespace

int main(int argc, char** argv) {
  std::string render_path;
#ifdef __ANDROID__
  // The app's private storage.
  std::string project_path = std::string(SDL_GetPrefPath("drumfake", "drumfake")) +
      "project.json";
#elif defined(__EMSCRIPTEN__)
  // IndexedDB storage, mounted by web/pre.js.
  std::string project_path = "/persist/project.json";
#else
  std::string project_path = DefaultProjectPath();
  MigrateLegacyDataDirectory(project_path);
#endif
  bool explicit_project = false;
  int bars = 4;
  int pattern = 0;
  bool grids = false;
  bool midi_monitor = false;
  AppOptions app_options;
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    bool has_value = i + 1 < argc;
    if (arg == "--render" && has_value) {
      render_path = argv[++i];
    } else if (arg == "--bars" && has_value) {
      bars = std::max(1, atoi(argv[++i]));
    } else if (arg == "--project" && has_value) {
      project_path = argv[++i];
      explicit_project = true;
    } else if (arg == "--pattern" && has_value) {
      pattern = std::max(1, std::min(kNumPatterns, atoi(argv[++i]))) - 1;
    } else if (arg == "--screenshot" && has_value) {
      app_options.screenshot_path = argv[++i];
    } else if (arg == "--theme" && has_value) {
      app_options.theme = argv[++i];
    } else if (arg == "--midi-monitor") {
      midi_monitor = true;
    } else if (arg == "--grids") {
      grids = true;
    } else {
      PrintUsage();
      return arg == "--help" || arg == "-h" ? 0 : 1;
    }
  }

  auto engine = std::make_unique<Engine>();
  engine->Init();
  engine->midi_monitor = midi_monitor;

  if (render_path.empty()) {
#ifdef DRUMFAKE_HEADLESS
    fprintf(stderr, "Built without GUI: use --render.\n");
    return 1;
#else
    app_options.project_path = project_path;
#ifdef __EMSCRIPTEN__
    // RunApp returns to the browser, which keeps running the app's frames.
    return RunApp(engine.release(), app_options);
#else
    return RunApp(engine.get(), app_options);
#endif
#endif
  }

  if (explicit_project) {
    std::string error;
    if (!LoadProject(project_path, &engine->project(), &error)) {
      fprintf(stderr, "%s\n", error.c_str());
      return 1;
    }
    engine->ReloadKit();
    std::vector<std::string> problems;
    LoadProjectSamples(engine.get(), project_path, &problems);
    for (const std::string& problem : problems) {
      fprintf(stderr, "Sample not loaded: %s\n", problem.c_str());
    }
  }
  engine->SelectPattern(pattern);
  engine->grids_enabled = grids;

  RenderStats stats;
  std::string error;
  if (!RenderBars(engine.get(), bars, render_path, &stats, &error)) {
    fprintf(stderr, "%s\n", error.c_str());
    return 1;
  }
  printf("Rendered %.2fs to %s in %.2fs (%.1fx real time)\n",
         stats.seconds, render_path.c_str(), stats.render_seconds,
         stats.seconds / std::max(1e-6, stats.render_seconds));
  printf("Peak %.3f, RMS %.3f, non-finite samples: %d\n",
         stats.peak, stats.rms, stats.non_finite);
  for (int t = 0; t < kNumTracks; ++t) {
    printf("  Track %c %-12s peak %.3f\n", 'A' + t,
           engine->project().sounds[t].name.c_str(), stats.track_peak[t]);
  }
  return stats.non_finite == 0 ? 0 : 2;
}
