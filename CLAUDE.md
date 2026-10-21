# Drumfake

Open-source groovebox (GPLv3) built on Mutable Instruments DSP: Plaits voices, Grids patterns,
Rings reverb and chorus. Independent project with its own design: no third-party brand or product
names (other than Mutable Instruments) in the UI, the docs or the code, and no faceplate copied
from an existing machine.

## Layout

- `third_party/eurorack` (unmodified submodule; never edit it), `third_party/SDL` (Android, web).
- `shims/`: minimal AVR headers so `grids/resources.cc` compiles on the desktop.
- `src/core/`: model (params, steps, patterns, per-pattern kits, song, scales), JSON I/O,
  Grids port, MIDI parser. No UI or audio device code.
- `src/audio/`: `TrackVoice` (Plaits + filter/drive/crush/choke), `MasterFx`, `Engine`
  (sequencer, clock, MIDI handling), `ModSources` (Peaks envelope + bouncing ball per track),
  `CloudsFx` (Clouds on the master mix).
  The `resources.h` of plaits, rings, peaks, marbles and clouds define the same macros: never
  include two of them in one translation unit (see `audio/buses.h`). Each of Peaks, Marbles and
  Clouds lives in its own `.cc` (`mod_sources`, `core/marbles_gen`, `clouds_fx`) behind a pimpl.
- `patches/`: header overrides that shadow the submodule (it is never edited), see its README.
- `src/ui/`: ImGui faceplate on a layout grid (`layout::` in `faceplate.cc`), widgets, and the
  multitouch layer (fingers captured by pads/knobs/buttons, one free finger drives ImGui).
- `src/io/`: MIDI input (RtMidi/ALSA on desktop, Java MidiManager + JNI on Android, Web MIDI
  in the browser).
- `android/`: Gradle project building the same CMakeLists.txt into `libmain.so`.
- `web/`: HTML shell and IndexedDB storage of the Emscripten build (published by
  `.github/workflows/web.yml` on GitHub Pages).

## Build, test, run

Full details in `docs/building.md` and `docs/development.md`.

```sh
cmake -B build && cmake --build build -j
./build/drumfake_tests        # must pass before any commit
./build/drumfake [--midi-monitor]
./build/drumfake --render out.wav --bars 4
./build/drumfake --screenshot out.ppm   # debug: renders frames, saves the window, quits
```

GCC prints messages in French: filter build output with `LANG=C`.

Web (Emscripten SDK in `~/emsdk`; the system `node` is too old, use `$EMSDK_NODE`):

```sh
source ~/emsdk/emsdk_env.sh
emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release && cmake --build build-web -j
$EMSDK_NODE build-web/drumfake_tests.js
python3 -m http.server -d build-web 8000   # http://localhost:8000/drumfake.html
```

Android (tablet over Wi-Fi adb, e.g. `adb -s 192.168.10.23:<port>`; the port changes):

```sh
cd android && ./gradlew assembleDebug
adb -s <device> install -r app/build/outputs/apk/debug/app-debug.apk
adb -s <device> shell am start -n org.drumfake.app/.DrumfakeActivity
```

## Conventions

- Mutable Instruments style: 2-space indent, `snake_case_` members, comments in English.
- UI model state is written by the UI thread under `engine.mutex()`; the audio thread reads it.
  Audio-to-UI values go through atomics. Keep work under the lock short.
- The audio thread flushes denormals (x86 FTZ/DAZ, ARM64 FPCR.FZ): keep it that way.
- New engine features get tests in `tests/tests.cc`; UI changes are checked with
  `--screenshot` (desktop) or `adb exec-out screencap` (tablet).
- Docs are in `docs/` (English): update `user-guide.md` and `controls.md` when behavior changes,
  `building.md` and `development.md` when the build or the architecture changes. `README.md` stays
  a short overview with links.
