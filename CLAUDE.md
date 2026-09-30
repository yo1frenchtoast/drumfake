# Drumf(re)ake

Open-source groovebox (GPLv3) inspired by the Arturia DrumFreak prototype and the DrumBrute
Impact workflow, built on Mutable Instruments DSP: Plaits voices, Grids patterns, Rings reverb
and chorus. Independent project: no Arturia branding in the UI (factual comments are fine).

## Layout

- `third_party/eurorack` (unmodified submodule; never edit it), `third_party/SDL` (Android).
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
- `src/io/`: MIDI input (RtMidi/ALSA on desktop, Java MidiManager + JNI on Android).
- `android/`: Gradle project building the same CMakeLists.txt into `libmain.so`.

## Build, test, run

```sh
cmake -B build && cmake --build build -j
./build/drumfreake_tests        # must pass before any commit
./build/drumfreake [--midi-monitor]
./build/drumfreake --render out.wav --bars 4
./build/drumfreake --screenshot out.ppm   # debug: renders frames, saves the window, quits
```

GCC prints messages in French: filter build output with `LANG=C`.

Android (tablet over Wi-Fi adb, e.g. `adb -s 192.168.10.23:<port>`; the port changes):

```sh
cd android && ./gradlew assembleDebug
adb -s <device> install -r app/build/outputs/apk/debug/app-debug.apk
adb -s <device> shell am start -n org.drumfreake.app/.DrumfreakeActivity
```

## Conventions

- Mutable Instruments style: 2-space indent, `snake_case_` members, comments in English.
- UI model state is written by the UI thread under `engine.mutex()`; the audio thread reads it.
  Audio-to-UI values go through atomics. Keep work under the lock short.
- The audio thread flushes denormals (x86 FTZ/DAZ, ARM64 FPCR.FZ): keep it that way.
- New engine features get tests in `tests/tests.cc`; UI changes are checked with
  `--screenshot` (desktop) or `adb exec-out screencap` (tablet).
- Update README.md (English, user-facing) when behavior changes.
