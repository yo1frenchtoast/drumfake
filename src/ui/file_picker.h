// Choosing a file to open, or where to save one, on each platform: the SDL
// dialogs on the desktop (the portal) and on Android (the Storage Access
// Framework), a file input and a download in the browser.
//
// The dialogs answer later, possibly on another thread: the UI thread picks up
// the answers with TakeFilePickerResults() once per frame.

#ifndef DRUMFAKE_UI_FILE_PICKER_H_
#define DRUMFAKE_UI_FILE_PICKER_H_

#include <string>
#include <vector>

namespace drumfake {

struct FilePickerResults {
  std::vector<std::string> opened;    // Files chosen with PickZipToOpen.
  std::vector<std::string> messages;  // What became of the files given to SaveZipAs.
};

// Asks for a .zip file to open. Its path (a content:// URI on Android) comes
// back in `opened`; nothing comes back if the dialog is cancelled.
void PickZipToOpen();

// Asks where to save `data`, a .zip file proposed as `name` (in the browser:
// downloads it). The outcome comes back in `messages`.
void SaveZipAs(const std::string& name, std::string data);

FilePickerResults TakeFilePickerResults();

// Reads a file given by PickZipToOpen or dropped on the window.
bool ReadWholeFile(const std::string& path, std::string* data, std::string* error);

}  // namespace drumfake

#endif  // DRUMFAKE_UI_FILE_PICKER_H_
