# Building Drumfake

## Desktop (Linux)

Requirements: a C++17 compiler, CMake 3.20 or later, SDL3, and nlohmann-json (it is downloaded
when missing). ALSA is needed for MIDI input. Dear ImGui and RtMidi are downloaded by CMake.

```sh
git clone --recursive https://github.com/yo1frenchtoast/drumfake
cd drumfake        # if you cloned without --recursive:
git submodule update --init --recursive
cmake -B build
cmake --build build -j
./build/drumfake_tests    # must pass before any commit
./build/drumfake          # the application
```

`third_party/eurorack` (the Mutable Instruments code) and `third_party/SDL` (used by the Android
build) are submodules. The build stops with a message when they are missing.

GCC prints its messages in French on some systems: prefix the build with `LANG=C` to get
English output you can filter.

### CMake options

| Option | Default | Effect |
|---|---|---|
| `DRUMFAKE_GUI` | `ON` | `OFF` builds the engine, the tests and the offline renderer only: no SDL3, ImGui or RtMidi needed. |

### Command line

```sh
./build/drumfake [--project FILE] [--midi-monitor]
./build/drumfake --render out.wav [--bars N] [--pattern N] [--grids] [--project FILE]
./build/drumfake --screenshot out.ppm
```

| Flag | Meaning |
|---|---|
| `--project FILE` | Project file to load and save. Default: `~/.local/share/drumfake/project.json` (or under `$XDG_DATA_HOME`). |
| `--midi-monitor` | Print every received MIDI message in the terminal. |
| `--render OUT.wav` | Render offline, without a window or an audio device. |
| `--bars N` | Length of the render, in bars (default 4). |
| `--pattern N` | Pattern to render, 1 to 16. |
| `--grids` | Play the Grids rhythm of the X/Y pad instead of the pattern steps. |
| `--screenshot OUT.ppm` | Debug: render a few frames, save the window as a PPM image, quit. |

## Android

Requirements: the Android SDK with NDK `30.0.16248370` and CMake `4.1.2` (install them with
`sdkmanager`), and a recent Java. SDL3 is built from the `third_party/SDL` submodule. The
Gradle project builds the same `CMakeLists.txt` into `libmain.so`.

```sh
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n org.drumfake.app/.DrumfakeActivity
```

The APK targets `arm64-v8a`, Android 8 (API 26) and later, which is where AAudio is available.
The app opens in full-screen landscape. Its project and kits are saved in the app's private
storage.

### Installing over Wi-Fi

With wireless debugging enabled on the tablet, the port changes at each pairing. Find it and
pass the device to every `adb` command:

```sh
adb mdns services                       # lists <ip>:<port>
adb connect 192.168.10.23:<port>
adb -s 192.168.10.23:<port> install -r app/build/outputs/apk/debug/app-debug.apk
adb -s 192.168.10.23:<port> exec-out screencap -p > screen.png
```

## Web

Requirements: the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
(tested with 6.0.13). SDL3 is built from the `third_party/SDL` submodule, as on Android.

```sh
source ~/emsdk/emsdk_env.sh
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
node build-web/drumfake_tests.js        # the test suite, compiled to WebAssembly
python3 -m http.server -d build-web 8000  # then open http://localhost:8000/drumfake.html
```

Use the `node` of the SDK (`$EMSDK_NODE`) if the system one is too old. The build writes
`drumfake.html`, `.js`, `.wasm` and `.data` (the fonts): serve the four files together, over
HTTPS or from `localhost` (Web MIDI needs a secure context). `.github/workflows/web.yml` builds
them on every push to `main` and publishes them on GitHub Pages (repository settings: Pages,
source *GitHub Actions*).

In the browser:

- The sound starts at the first click, touch or key press (browsers block audio until then).
- The project and the kits are saved in the browser's storage (IndexedDB) for this site: they
  survive a reload, not clearing the site data.
- MIDI inputs work in browsers that have Web MIDI (Chrome, Edge, Firefox), after the permission
  prompt.
- The command-line flags do not apply.

## Troubleshooting

- **`Submodules missing`**: run `git submodule update --init --recursive`.
- **No sound on Linux**: SDL3 picks the default audio device; check that it is the one you
  expect (for example with `pavucontrol`).
- **No MIDI port listed**: make sure ALSA sequencer devices exist (`aconnect -l`).
- **Web: no sound**: click the page once; check that the tab is not muted.
- **Web: no MIDI**: allow MIDI for the site in the browser's permissions, and use HTTPS or
  `localhost`.
