# Development guide

How the code is organised, the rules that keep it working, and recipes for the usual changes.
To build and run, see [building](building.md).

## Layout

```
third_party/eurorack   Mutable Instruments code (submodule, never edited)
third_party/SDL        SDL3 (submodule, used by the Android and web builds)
shims/                 minimal AVR headers so grids/resources.cc compiles on a desktop
patches/               header overrides that shadow the submodule (see patches/README.md)
src/core/              model and logic with no UI or audio device: params, steps, patterns,
                       kits, song, scales, JSON I/O, Grids and Marbles ports, MIDI parser
src/audio/             TrackVoice, SamplePlayer, GrainPlayer, MasterFx, CloudsFx, ModSources, Engine,
                       sample loading, WAV renderer
src/ui/                ImGui faceplate, widgets, multitouch layer
src/io/                MIDI input: RtMidi/ALSA (desktop), Java MidiManager + JNI (Android),
                       Web MIDI (browser)
src/app.cc, main.cc    window and audio device setup, command line
android/               Gradle project building the same CMakeLists.txt into libmain.so
web/                   HTML shell and storage setup of the Emscripten build
tests/tests.cc         the test suite (one executable, no framework)
docs/                  this documentation
```

The CMake targets: `mutable_dsp` (the Mutable Instruments sources, built as a static library
with `-w`), `drumfake_core` (everything in `src/core` and `src/audio`, built with `-Wall
-Wextra`), `drumfake_tests`, and `drumfake` (the application).

## Data flow

```
 UI thread (ImGui)                         audio thread (SDL callback)
 ─────────────────                         ───────────────────────────
 Faceplate  ── writes under lock ──►  Project (params, patterns, kits, song…)
                                          │
                                      Engine::Render(): RenderBlock() per 12 frames
                                          │  sequencer → FireTrack/FireStep
                                          │  per track: ModSources → ComputeParams → TrackVoice::Render
                                          │  chords: FireChord → pool of 4 extra TrackVoices, rendered with the params of their track
                                          │  buses → MasterFx → CloudsFx → click
                                          ▼
 Faceplate  ◄── reads atomics ───  playhead, meters, scope, effective params, CPU load
```

### Threading rules

- `Project` and every non-atomic member of `Engine` are shared between the two threads. The UI
  holds `engine.mutex()` while it reads or writes them; `Engine::Render()` takes the same lock.
  Keep the work done under the lock short.
- Audio-to-UI values (playhead, peaks, scope, active locks, CPU load) go through atomics.
- Nothing allocates, logs or does I/O on the audio thread. Buffers are sized in `Init()`.
- The audio thread flushes denormals (x86 FTZ/DAZ, ARM64 FPCR.FZ). Keep it that way: decaying
  reverb tails otherwise make x86 rendering slow in bursts.
- Timing: audio is rendered in blocks of 12 samples at 48 kHz (`kBlockSize`, the Plaits block),
  so the sequencer is accurate to 0.25 ms.

### Parameters

Every track parameter is a float normalized to [0, 1] in `TrackSound::params`, indexed by the
`TrackParam` enum (`src/core/params.h`). The engine maps it to real units; `ParamInfo` and
`FormatValue()` print it on the screen. The first `kNumLockable` parameters can be
parameter-locked per step and used as modulation destinations. The others (LFO, Envs) are not.
`ComputeParams()` in `engine.cc` builds the *effective* values of a block: knob value, then
step lock, then LFOs, Peaks sources and macros.

Master (`M_*`), assign (`A_*`) and Clouds (`C_*`) parameters are project-wide arrays with the
same normalized convention.

## Using the Mutable Instruments code

The submodule is **never edited**. Three rules follow.

1. **Never include two `resources.h` in one translation unit.** The ones of Plaits, Rings,
   Peaks, Marbles and Clouds define the same macros (`LUT_*`…) with different values. The
   modules that would clash are therefore isolated in their own `.cc` file, behind a header
   that includes no Mutable Instruments header, usually with a pimpl:

   | Module | Isolated in |
   |---|---|
   | Plaits | `audio/track_voice.*` |
   | Rings | `audio/master_fx.*` |
   | Peaks | `audio/mod_sources.*` |
   | Clouds | `audio/clouds_fx.*` |
   | Grids | `core/grids_gen.*` |
   | Marbles | `core/marbles_gen.*` |

   `audio/buses.h` holds the shared constants (`kSampleRate`, `kBlockSize`) for this reason.
2. **Fix upstream bugs with an override, not an edit.** `patches/` precedes the submodule in
   the include path, so a file placed there shadows the original. Document each one in
   `patches/README.md`. There is one: Clouds' `window.h`.
3. **Zero what the firmware relied on being zero.** The modules are written for firmware
   globals, which start zeroed. Allocate their objects value-initialized (`new Impl()`), as in
   `CloudsFx`, and set flags explicitly when `Init()` does not.

### Adding another module

1. Add its `.cc` files to `mutable_dsp` in `CMakeLists.txt` (list the `resources.cc` and only
   the DSP sources you use; tables such as `stmlib/dsp/atan.cc` may be needed too).
2. Write `src/<dir>/<name>.{h,cc}`: the header exposes a small class with no Mutable
   Instruments types, the `.cc` holds the include and the `Impl`.
3. Drive it from the `Engine` (audio) or from the model (offline generators like Marbles and
   Grids), add its parameters to `params.h`, and save them in `project_io.cc`.
4. Add tests, run them, and check the CPU cost on the tablet (`CPU` is on the screen).

## Common changes

### Add a track parameter

1. Add the enum value to `TrackParam` and its `ParamInfo` in `kTrackParams` (same order), in
   `params.h` and `params.cc`. A new format goes in `ParamFormat` and `FormatValue()`.
2. Give it a default that changes nothing: old projects have no value for it.
3. Use it: in `TrackVoice::Render()` for sound, in `ComputeParams()` for modulation.
4. If it lives on a page, make sure `page_param_info()` and `KnobTarget()` (in
   `faceplate.cc`) map the page's knobs to it.
5. Add a test.

The project file stores arrays by index and tolerates arrays that are shorter than the code
expects, so appending parameters keeps old projects loading. **Do not reorder or insert
parameters** in the middle of an enum without a migration.

### Add a page of knobs

Add it to the `Page` enum (before `PAGE_ASSIGN`, which is last), then update: `kPageNames` and
`page_param_info()` in `params.cc`; `KnobTarget()`, the icon table in `DrawPageButtons()` and
the key table in `HandleKeyboard()` in `faceplate.cc`; and a new `Icon` in `widgets.{h,cc}`.
Check the width of the page buttons with a screenshot.

### Add a screen on the display

The display shows one view at a time (`ScreenView` in `faceplate.h`). Add a value, a
`DrawXxxView(min, max)` method, the dispatch in `DrawScreen()`, and a way to enter it (a button
in `DrawEditView()` for example). The *Back* button returns to `VIEW_MAIN`. `DrawMarblesView()`
and `DrawKitsView()` are short examples.

### The sampler

```
core/sample_file    WAV decoding to mono 16-bit 48 kHz (windowed-sinc resampling), 60 s max
core/sample_pool    Project::samples (128 slots naming files of samples/), copying files in,
                    loading the pool (SamplePoolLoad), the slot of a sound (SampleSlot = Aux)
audio/sample_player Start / Length (bipolar, backwards) / Loop / release, Hermite interpolation
audio/grain_player  Scrub / Spray / Burst: up to 16 windowed grains, chaos from a per-voice
                    xorshift sequence (seeded by TrackVoice::Init), so renders are repeatable
audio/sample_read.h the Hermite read shared by both players
core/slicing        SampleSlices (up to 48 starts per pool slot, Project::slices, saved with
                    the pool), equal slices, attack detection, Colour <-> slice mapping
audio/sample_loader LoadProjectSamples, ImportSample, ApplySampleDefaults (engine glue)
```

- The audio lives in `Engine::samples_` (`shared_ptr<const SampleData>` per slot). Every block,
  `RenderBlock` hands each voice the sample of the slot in its effective params, so a sample
  replaced while playing never dangles (the player clamps its position).
- Samples are decoded **without** the lock, swapped in **under** it (`SwapSample`,
  `SwapSamples`), and the previous ones released after it: never on the audio thread. The
  faceplate draws under the lock, so it queues file work (imports, Reload saved) and does it in
  `DoFileWork()` after drawing.
- Engines are only ever added at the end of the list: projects and kit files record
  `engine_count`, and loading remaps engine values and engine locks saved with another count
  (`RemapEngine` in `project_io.cc`; older files had the 24 Plaits engines).
- `TestSampleTracks` checks with a counting allocator that `Engine::Render` never allocates.

### Change the project file

`project_io.cc` writes and reads JSON (`kFormatVersion`). Read every new field with a default
(`j.value("name", default)`) and clamp it, so old files load. Add the field to the round trip
test in `tests.cc`.

## Testing

```sh
./build/drumfake_tests
```

The suite is one executable (`tests/tests.cc`) with no framework: `CHECK(cond)` and
`CHECK_NEAR(value, expected, tolerance)` count failures, and the exit code is non-zero when one
fails. **It must pass before every commit.** Useful helpers:

- `MakeEngine()`: an engine with an empty pattern 1 at 120 BPM (6000 samples per step);
- `RenderAndCollect(engine, track, frames)`: renders and returns the sample time of each trigger;
- read `engine->effective_param[track][param]` to check modulations and locks.

Every engine feature gets tests. For DSP integrations, test the behavior that matters to the
user (it plays sound, it is finite, the same seed gives the same result), not the internals of
the Mutable Instruments code.

### Checking the UI

- Desktop: `./build/drumfake --screenshot out.ppm` renders a few frames and writes the
  window. Convert it to PNG to look at it (`python3 -c "from PIL import Image;
  Image.open('out.ppm').save('out.png')"`). To capture a given page or view, set the default
  of `page_` or `view_` in `faceplate.h` temporarily.
- Tablet: `adb -s <device> exec-out screencap -p > screen.png`.

### Offline renders

`--render` runs the engine without a window: handy to listen to a change, to compare two
versions (same pattern, same seed), or to measure the CPU load in a script.

## Conventions

- Mutable Instruments style: 2-space indent, `snake_case_` members, comments in English.
- Code reads like its neighbours: same comment density, naming and idioms.
- UI model state is written by the UI thread under `engine.mutex()`; the audio thread reads it.
- New engine features get tests; UI changes are checked with a screenshot.
- Update the [user guide](user-guide.md) and [controls](controls.md) when a behavior changes.
- The project is GPLv3 and independent: no third-party brand or product names in the UI, the
  docs or the code.

## Android notes

- The same `CMakeLists.txt` builds `libmain.so`; the Gradle project only packages it.
- Audio uses AAudio through SDL3 (`minSdk` 26). MIDI goes through Java's `MidiManager`, with
  JNI into `src/io/midi_android.cc`.
- Fingers are captured by pads, knobs and buttons; one free finger drives ImGui
  (`src/ui/faceplate.cc`, `FingerDown` and friends). Touch targets matter: test on the tablet.
- Fonts (Noto Sans, OFL) are bundled in `android/app/src/main/assets/fonts`.

## Web notes

- `emcmake cmake` builds the same targets with Emscripten: `drumfake.html` and a
  `drumfake_tests.js` that runs under Node.js. SDL3 comes from the submodule, statically.
- There are no threads: the browser drives the main loop (`emscripten_set_main_loop_arg` in
  `src/app.cc`, one `Frame()` per animation frame) and SDL renders audio on the main thread,
  between frames, with 1024-frame buffers. The engine lock is still taken, it just never waits.
- WebAssembly cannot flush denormals: the FTZ/DAZ setup in `Engine::Render()` does not apply.
- `web/pre.js` mounts `/persist` (IndexedDB) before `main()` runs and writes it back every three
  seconds and when the page is hidden. The project is `/persist/project.json`.
- MIDI: `src/io/midi_web.cc` asks for Web MIDI access and hands each message to `DeliverMidi`.
- The fonts are the Android ones, preloaded into `/fonts`.
