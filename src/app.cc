#include "app.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <dlfcn.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#include "core/project_io.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "ui/faceplate.h"

namespace drumfreake {

namespace {

// The few GL calls made outside ImGui, resolved at run time so that the
// executable does not link against a particular libGL.
struct GlFunctions {
  void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
  void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
  void (*Clear)(GLbitfield);
  void (*PixelStorei)(GLenum, GLint);
  void (*ReadBuffer)(GLenum);
  void (*ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);

  bool Load() {
    Viewport = reinterpret_cast<decltype(Viewport)>(SDL_GL_GetProcAddress("glViewport"));
    ClearColor = reinterpret_cast<decltype(ClearColor)>(SDL_GL_GetProcAddress("glClearColor"));
    Clear = reinterpret_cast<decltype(Clear)>(SDL_GL_GetProcAddress("glClear"));
    PixelStorei = reinterpret_cast<decltype(PixelStorei)>(SDL_GL_GetProcAddress("glPixelStorei"));
    ReadBuffer = reinterpret_cast<decltype(ReadBuffer)>(SDL_GL_GetProcAddress("glReadBuffer"));
    ReadPixels = reinterpret_cast<decltype(ReadPixels)>(SDL_GL_GetProcAddress("glReadPixels"));
    return Viewport && ClearColor && Clear && PixelStorei && ReadBuffer && ReadPixels;
  }
};

GlFunctions gl;

// Homebrew's Mesa cannot drive every GPU (NVIDIA, for instance). Load the
// system's GL dispatch libraries first: SDL and ImGui open these libraries by
// soname, so they will reuse the ones already loaded.
void PreferSystemGlLibraries() {
#if defined(__linux__) && !defined(__ANDROID__)
  const char* const kDirectories[] = {
    "/usr/lib64/", "/usr/lib/x86_64-linux-gnu/", "/usr/lib/aarch64-linux-gnu/", "/usr/lib/"
  };
  const char* const kLibraries[] = {
    "libGLdispatch.so.0", "libOpenGL.so.0", "libEGL.so.1", "libGLX.so.0", "libGL.so.1"
  };
  for (const char* library : kLibraries) {
    for (const char* directory : kDirectories) {
      std::string path = std::string(directory) + library;
      if (std::filesystem::exists(path)) {
        dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
        break;
      }
    }
  }
#endif
}

// Pulls audio from the engine whenever SDL needs more.
void AudioCallback(void* user_data, SDL_AudioStream* stream,
                   int additional_amount, int /*total_amount*/) {
  static std::vector<float> buffer(8192 * 2);
  Engine* engine = static_cast<Engine*>(user_data);
  int frames = additional_amount / static_cast<int>(2 * sizeof(float));
  while (frames > 0) {
    int chunk = std::min(frames, 8192);
    engine->Render(buffer.data(), static_cast<size_t>(chunk));
    SDL_PutAudioStreamData(stream, buffer.data(), chunk * 2 * static_cast<int>(sizeof(float)));
    frames -= chunk;
  }
}

ImFont* LoadFont(const char* const* candidates, float size) {
  ImGuiIO& io = ImGui::GetIO();
#ifdef __ANDROID__
  // Fonts are bundled in the APK's assets, which SDL reads by relative path.
  for (const char* const* path = candidates; *path; ++path) {
    size_t length = 0;
    void* data = SDL_LoadFile(*path, &length);
    if (!data) {
      continue;
    }
    // The atlas frees the font data with ImGui's allocator.
    void* copy = IM_ALLOC(length);
    memcpy(copy, data, length);
    SDL_free(data);
    ImFontConfig config;
    config.OversampleH = 2;
    return io.Fonts->AddFontFromMemoryTTF(copy, static_cast<int>(length), size, &config);
  }
#endif
  for (const char* const* path = candidates; *path; ++path) {
    if (std::filesystem::exists(*path)) {
      ImFontConfig config;
      config.OversampleH = 2;
      return io.Fonts->AddFontFromFileTTF(*path, size, &config);
    }
  }
  ImFontConfig config;
  config.SizePixels = size;
  return io.Fonts->AddFontDefault(&config);
}

void SetupStyle() {
  ImGuiStyle& style = ImGui::GetStyle();
  ImGui::StyleColorsDark();
  style.FrameRounding = 2.0f;
  style.GrabRounding = 2.0f;
  style.WindowPadding = ImVec2(0, 0);
  style.WindowBorderSize = 0.0f;
  // Screen widgets, in the faceplate's colors (see ui/widgets.h).
  const ImVec4 kAccent = ImGui::ColorConvertU32ToFloat4(colors::kAccent);
  const ImVec4 kAccentLight(0.50f, 0.88f, 0.78f, 1.0f);
  ImVec4* c = style.Colors;
  c[ImGuiCol_FrameBg] = ImVec4(0.09f, 0.14f, 0.16f, 1.0f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.19f, 0.21f, 1.0f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.15f, 0.24f, 0.26f, 1.0f);
  c[ImGuiCol_SliderGrab] = kAccent;
  c[ImGuiCol_SliderGrabActive] = kAccentLight;
  c[ImGuiCol_CheckMark] = kAccent;
  c[ImGuiCol_Button] = ImVec4(0.12f, 0.19f, 0.21f, 1.0f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.16f, 0.26f, 0.28f, 1.0f);
  c[ImGuiCol_ButtonActive] = kAccent;
  c[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(colors::kScreenText);
}

bool SaveScreenshot(const std::string& path, int width, int height) {
  std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
  gl.PixelStorei(GL_PACK_ALIGNMENT, 1);
  gl.ReadBuffer(GL_BACK);
  gl.ReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) {
    return false;
  }
  fprintf(f, "P6\n%d %d\n255\n", width, height);
  for (int y = height - 1; y >= 0; --y) {
    fwrite(&pixels[static_cast<size_t>(y) * width * 3], 1, static_cast<size_t>(width) * 3, f);
  }
  fclose(f);
  return true;
}

}  // namespace

int RunApp(Engine* engine, const AppOptions& options) {
  const std::string& project_path = options.project_path;
  const bool screenshot = !options.screenshot_path.empty();
  if (std::filesystem::exists(project_path)) {
    std::string error;
    if (!LoadProject(project_path, &engine->project(), &error)) {
      fprintf(stderr, "%s (starting with the default project)\n", error.c_str());
    }
  }

  PreferSystemGlLibraries();
  SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "256");
  // Fingers are handled by the faceplate (multitouch), not turned into mouse
  // events.
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_AudioSpec spec;
  spec.format = SDL_AUDIO_F32;
  spec.channels = 2;
  spec.freq = static_cast<int>(kSampleRate);
  SDL_AudioStream* stream = screenshot ? nullptr : SDL_OpenAudioDeviceStream(
      SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, AudioCallback, engine);
  if (!stream && !screenshot) {
    fprintf(stderr, "No audio output: %s\n", SDL_GetError());
  } else {
    SDL_ResumeAudioStreamDevice(stream);
  }

#ifdef __ANDROID__
  const char* glsl_version = "#version 300 es";
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  const SDL_WindowFlags kExtraWindowFlags = SDL_WINDOW_FULLSCREEN;
  // Keep the screen on while the app runs.
  SDL_DisableScreenSaver();
#else
  const char* glsl_version = "#version 130";
  const SDL_WindowFlags kExtraWindowFlags = 0;
#endif
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
  SDL_Window* window = SDL_CreateWindow(
      "Drumf(re)ake", 1400, 773,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | kExtraWindowFlags);
  if (!window) {
    // Retry without multisampling.
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    window = SDL_CreateWindow("Drumf(re)ake", 1400, 773,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | kExtraWindowFlags);
  }
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (!gl_context) {
    fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_GL_MakeCurrent(window, gl_context);
  SDL_GL_SetSwapInterval(1);
  if (!gl.Load()) {
    fprintf(stderr, "Cannot resolve OpenGL functions\n");
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  SetupStyle();

  static const char* const kRegular[] = {
    "fonts/NotoSans-Regular.ttf",  // Android assets.
    "/usr/share/fonts/google-noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    nullptr
  };
  static const char* const kBold[] = {
    "fonts/NotoSans-Bold.ttf",  // Android assets.
    "/usr/share/fonts/google-noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Bold.ttf",
    "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    nullptr
  };
  ImFont* font = LoadFont(kRegular, 28.0f);
  ImFont* bold = LoadFont(kBold, 28.0f);

  ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
  if (!ImGui_ImplOpenGL3_Init(glsl_version)) {
    fprintf(stderr, "Cannot initialize the OpenGL renderer\n");
    return 1;
  }

  Faceplate faceplate;
  faceplate.Init(engine, project_path, font, bold);

  bool done = false;
  int frame = 0;
  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_MOTION ||
          event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_CANCELED) {
        // Finger positions are normalized to the window.
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window, &width, &height);
        ImVec2 position(event.tfinger.x * width, event.tfinger.y * height);
        uint64_t finger = static_cast<uint64_t>(event.tfinger.fingerID);
        if (event.type == SDL_EVENT_FINGER_DOWN) {
          faceplate.FingerDown(finger, position);
        } else if (event.type == SDL_EVENT_FINGER_MOTION) {
          faceplate.FingerMove(finger, position);
        } else {
          faceplate.FingerUp(finger);
        }
        continue;
      }
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
           event.window.windowID == SDL_GetWindowID(window))) {
        done = true;
      }
    }
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(10);
      continue;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Drumf(re)ake", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushFont(font);
    faceplate.Draw();
    ImGui::PopFont();
    ImGui::End();

    ImGui::Render();
    gl.Viewport(0, 0, static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x),
               static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y));
    gl.ClearColor(0.09f, 0.09f, 0.10f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (screenshot && ++frame >= options.screenshot_frames) {
      int width = 0;
      int height = 0;
      SDL_GetWindowSizeInPixels(window, &width, &height);
      if (!SaveScreenshot(options.screenshot_path, width, height)) {
        fprintf(stderr, "cannot write %s\n", options.screenshot_path.c_str());
      }
      done = true;
    }
    SDL_GL_SwapWindow(window);
  }

  if (stream) {
    SDL_DestroyAudioStream(stream);
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_GL_DestroyContext(gl_context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

}  // namespace drumfreake
