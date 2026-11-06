#include "ui/file_picker.h"

#include <memory>
#include <mutex>
#include <utility>

#include <SDL3/SDL.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace drumfake {

namespace {

std::mutex g_mutex;
FilePickerResults g_results;

void AddOpened(const std::string& path) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_results.opened.push_back(path);
}

void AddMessage(const std::string& message) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_results.messages.push_back(message);
}

// The name to show for a path or a content:// URI.
std::string DisplayName(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  // content://.../document/primary%3ADownload%2Fx.zip: keep what follows the last %2F.
  size_t encoded = name.rfind("%2F");
  return encoded == std::string::npos ? name : name.substr(encoded + 3);
}

}  // namespace

FilePickerResults TakeFilePickerResults() {
  std::lock_guard<std::mutex> lock(g_mutex);
  FilePickerResults results;
  std::swap(results, g_results);
  return results;
}

bool ReadWholeFile(const std::string& path, std::string* data, std::string* error) {
  size_t size = 0;
  void* bytes = SDL_LoadFile(path.c_str(), &size);
  if (!bytes) {
    *error = "cannot read " + DisplayName(path) + ": " + SDL_GetError();
    return false;
  }
  data->assign(static_cast<const char*>(bytes), size);
  SDL_free(bytes);
  return true;
}

#ifdef __EMSCRIPTEN__

// The browser has no dialog SDL can open: a hidden file input reads the file
// into /tmp, and a link downloads the saved file. Both run right after the
// click on the button, while the browser still counts it as a user gesture.

}  // namespace drumfake

EM_JS_DEPS(file_picker, "$UTF8ToString,$FS");

extern "C" EMSCRIPTEN_KEEPALIVE void drumfake_web_file_opened() {
  drumfake::AddOpened("/tmp/drumfake-import.zip");
}

EM_JS(void, WebPickFile, (), {
  var input = document.createElement("input");
  input.type = "file";
  input.accept = ".zip,application/zip";
  input.onchange = function() {
    var file = input.files && input.files[0];
    if (!file) {
      return;
    }
    file.arrayBuffer().then(function(buffer) {
      FS.writeFile("/tmp/drumfake-import.zip", new Uint8Array(buffer));
      _drumfake_web_file_opened();
    });
  };
  input.click();
});

EM_JS(void, WebDownload, (const char* name, const char* data, int size), {
  var blob = new Blob([HEAPU8.slice(data, data + size)], { type: "application/zip" });
  var link = document.createElement("a");
  link.href = URL.createObjectURL(blob);
  link.download = UTF8ToString(name);
  document.body.appendChild(link);
  link.click();
  link.remove();
  setTimeout(function() { URL.revokeObjectURL(link.href); }, 10000);
});

namespace drumfake {

void PickZipToOpen() {
  WebPickFile();
}

void SaveZipAs(const std::string& name, std::string data) {
  WebDownload(name.c_str(), data.data(), static_cast<int>(data.size()));
  AddMessage("Downloaded " + name);
}

#else

namespace {

const SDL_DialogFileFilter kZipFilter[] = { { "Drumfake project (.zip)", "zip" } };

struct PendingSave {
  std::string data;
};

void SDLCALL OnOpenChosen(void*, const char* const* files, int) {
  if (!files) {
    AddMessage(std::string("Cannot open the file dialog: ") + SDL_GetError());
  } else if (files[0]) {
    AddOpened(files[0]);
  }
}

void SDLCALL OnSaveChosen(void* userdata, const char* const* files, int) {
  std::unique_ptr<PendingSave> save(static_cast<PendingSave*>(userdata));
  if (!files) {
    AddMessage(std::string("Cannot open the file dialog: ") + SDL_GetError());
    return;
  }
  if (!files[0]) {
    return;  // Cancelled.
  }
  std::string path = files[0];
  // The portal may leave the extension out; a content:// URI has its own type.
  if (path.rfind("content://", 0) != 0 &&
      (path.size() < 4 || SDL_strcasecmp(path.c_str() + path.size() - 4, ".zip") != 0)) {
    path += ".zip";
  }
  if (SDL_SaveFile(path.c_str(), save->data.data(), save->data.size())) {
    AddMessage("Exported " + DisplayName(path));
  } else {
    AddMessage("Export failed: " + std::string(SDL_GetError()));
  }
}

}  // namespace

void PickZipToOpen() {
  SDL_ShowOpenFileDialog(OnOpenChosen, nullptr, nullptr, kZipFilter, 1, nullptr, false);
}

void SaveZipAs(const std::string& name, std::string data) {
  // Proposed in the home folder, under its name (the portal needs a full path).
  std::string location = name;
  if (const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME)) {
    location = std::string(home) + name;
  }
  auto* save = new PendingSave{ std::move(data) };
  SDL_ShowSaveFileDialog(OnSaveChosen, save, nullptr, kZipFilter, 1, location.c_str());
}

#endif

}  // namespace drumfake
