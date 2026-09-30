# Drumf(re)ake

Open-source groovebox inspired by the Arturia DrumFreak prototype (not commercially released to
date). Independent project, not affiliated with Arturia. It is built on the open-source Mutable
Instruments DSP (unmodified `third_party/eurorack` submodule):

- **Plaits**: one voice per track (8 tracks, 24 engines, including analog or synthetic kick,
  snare and hi-hat, modal, FM…);
- **Grids**: algorithmic rhythm generation on the X/Y pad ("Spice" mode);
- **Rings**: reverb and chorus on the master bus;
- **Peaks**: envelope and bouncing ball modulation sources;
- **Marbles**: random rhythm generator ("t" section) written into the steps;
- **Clouds**: granular, time-stretch, looping delay and spectral effect on the master mix.

## Building

```sh
git submodule update --init --recursive
cmake -B build
cmake --build build -j
./build/drumfreake_tests   # tests for the sequencer, Grids, saving and rendering
./build/drumfreake         # application
```

Dependencies: SDL3 and nlohmann-json (Homebrew on this system). Dear ImGui is downloaded by CMake.
`-DDRUMFREAKE_GUI=OFF` builds a version without a user interface.

### Android

Requirements: Android SDK with NDK 30.0.16248370 and CMake 4.1.2 (`sdkmanager`), a recent Java.
SDL3 is built from the `third_party/SDL` submodule.

```sh
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Everything is multitouch: several pads at once, a pad and a knob, the X/Y pad, and button
combinations (*Last Step* or *Copy* + pages, *Erase* + knob…). A long press (0.5 s) replaces the
right click (selecting a step, editing a Chord on pads 1–8) and a double tap on a knob resets it
to its default value.

The APK (arm64-v8a, Android 8 minimum) opens in full-screen landscape. The Noto Sans fonts
(OFL license) are bundled, and the project is saved in the app's private storage.

Offline rendering:

```sh
./build/drumfreake --render demo.wav --bars 4 [--grids] [--pattern N] [--project file.json]
```

## Controls

| Faceplate | Mouse | Keyboard |
|---|---|---|
| Pads A–H | click = plays and selects the track (Shift = accent) | `Q W E R T Y U I` |
| Pads 1–8 | depends on the mode: Chords, Sounds, Repeats, keyboard | `A S D F G H J K` |
| Knobs | drag vertically (Shift = fine), wheel, double-click = default | |
| Steps 1–16 | click = on/off; right click = select the step (p-lock) | |
| Play / Rec | | `Space` / `Enter` |
| Pages Sound…Assign | | `1`–`8` |
| Step pages A–D | | `Z` / `X` |
| Last Step (held) / Poly / Shift (held) | | `L` / `P` / `Alt` |
| Save, Undo, Copy, Paste, Erase | | `Ctrl+S`, `Ctrl+Z`, `Ctrl+C`, `Ctrl+V`, `Delete` (on release) |
| Back (↺) | closes Edit/Browse, releases the selected step | `Esc` |

- **Shift** (bottom left, or `Alt`) held, as on the DrumBrute: the steps become options. 1 to 5:
  pattern time division (1/8, 1/8T, 1/16, 1/16T, 1/32); 13: metronome; 14: *At End* (switch
  pattern at the end or immediately); 15: steps follow the playing page; 16: Poly. Shift + pad:
  select a track without playing it.
- **Held step** (by finger, 0.5 s; right click with the mouse to keep it selected): while it is
  held, the knobs set parameter locks, pads 1–4 set its repeats (1 to 4 hits) and the encoder
  shifts it early or late (±50% of a step, in 5% increments, amber line under the step). When
  released, you are back to normal editing.
- **Last Step** held (or `L`): the steps show the length; pressing a step sets the last step of
  the pattern (of the selected track in Poly), a page sets it to 16/32/48/64.
- **Parameter locks**: right click a step, then turn a knob (orange ring = locked; double-click
  to remove the lock). The screen lets you edit velocity, probability, ratchets and note.
- **Pages A|16 to D|64**: during playback, the steps follow the page played by the selected
  track; choosing a page by hand freezes the display until the next Play. *Last Step* held + a
  page = pattern length (16, 32, 48 or 64 steps). *Copy* held + a source page + a destination
  page: for example A|16 then D|64 repeats the first 16 steps (with their locks, on all tracks)
  up to step 64 and extends the pattern; a destination before the source simply copies the page.
  *Last Step* and *Copy* released alone keep their usual action.
- **Polyrhythm**: in Poly, *Last Step* + a step = last step of the selected track (pressing the
  same step again = pattern length). *Poly* (per pattern): each track loops on its own length
  without resyncing at the end of the pattern; everything realigns on Play and on pattern
  change. Without Poly, short tracks restart with the pattern.
- **Knob visual feedback**: orange halo = the current note plays a lock on this parameter; arc +
  dot = difference between the knob and the value actually played (orange for a lock, blue for
  an LFO or a macro); orange dot = at least one step of the track locks this parameter.
- **Erasing automation**: hold *Erase* (or `Delete`) and touch a knob to erase the locks of this
  parameter on all steps of the selected track (red dots while held; *Undo* to revert).
  Released alone, *Erase* keeps its usual action.
- **Motion recording**: Rec + Play, then turn a knob: the lock is written on the current step.
- **X/Y pad**: *Spice* = position in the Grids map (click Spice again to turn it on/off;
  tracks A–C, densities and chaos on the Assign page); *FX* = DJ filter (X) and delay send (Y);
  *Looper* = sequencer loop while the pad is touched (Y = length); *Macros* = parameters
  assigned on the Assign page.
- **Pads 1–8**: *Chords* plays track combinations (right click a pad to edit it); *Sounds* loads
  a factory sound (click Sounds again = bank 2); *Repeats* repeats the held tracks at the chosen
  division; *Keys* plays the selected track on the chosen scale and writes the note of the
  selected step.
- **Keys mode**: root, scale (major, minor, modes, harmonic minor, pentatonics, blues,
  chromatic) and octave, shared by the whole project. The *Keys* button shows a "Keys" page
  (knobs 1–3 = root, scale, octave; click Keys again to go back to the sound pages); the screen
  shows the settings and the pad notes, and the encoder changes the selected field. Shortcuts:
  `←`/`→` octave, `↑`/`↓` root, `Tab`/`Shift+Tab` scale. Notes are added to the track's Pitch: a
  track set to C plays in the chosen key.
- **Envs page** (Peaks modulation sources, one set per track, restarted by every note): an
  *AD envelope* (attack, decay, bipolar amount, destination) and a *bouncing ball* (gravity,
  bounce, amount, destination; a higher bounce is more elastic, so the ball takes longer to come
  to rest). Each one can drive any lockable parameter, like the LFOs of the Mods page: a full
  amount sweeps the whole range of the destination. The ball gives accelerating or decelerating
  rolls on a filter cutoff or a pitch.
- **Clouds page** (master mix, project-wide): *Mode* (Granular, Stretch, Looping delay,
  Spectral), *Pos*, *Size*, *Pitch* (±24 semitones), *Dens*, *Text*, *Fdbk* and *D/W*. It is off
  until *D/W* is raised. The meaning of Pos/Size/Dens/Text depends on the mode, as on the
  module (in Granular, Dens at 50% plays no grain: turn it either way). The **Freeze** button
  (top row, where the unemulated Sample button was) stops recording and loops the last
  ~2 seconds. Clouds runs at 48 kHz instead of its native 32 kHz: its times are 2/3 of the
  module's. Its own reverb is off, the master reverb does the job.
- **Random rhythm** (Marbles): *Edit* → *Random rhythm...*. Pick a *model* (Complementary,
  Clusters, Drums, Independent, Divider, Three states, Markov), a *Bias* (density or pattern
  choice, depending on the model), a *Deja vu* amount (0%: always new; 100%: a loop of *Loop*
  steps that repeats) and the two tracks that receive the outputs t1 and t3, then press
  *Generate*: the steps of both tracks are replaced over the pattern length (*Undo* reverts).
  Press it again for a new draw, then edit the steps as usual.
- **Encoder**: changes the track's Plaits engine (or the sound in Browse).
- **Song** (pattern chain, as on the Arturia units): press *Song* (it blinks), then the step
  buttons 1–16 in the desired order; a pattern can appear several times (e.g. 1-1-2-3), up to 64
  entries. The first press starts a new chain; *Undo* removes the last entry, *Erase* clears the
  chain. Pressing *Song* again loops the chain (from the start on Play; during playback, after
  the current pattern). Choosing a pattern in *Patterns* mode leaves Song mode. The chain is
  saved with the project.
- **Kits**: each pattern has its own sounds, loaded with it (including in Song mode); knob
  settings stay in the current pattern. *Copy*/*Paste* of a pattern also copies its kit.
  **Kit library** (*Edit* → *Kit library...*): kits saved as files, shared by all projects
  (`kits/` beside `project.json`). *Save new* stores the current 8 sounds as the next "Kit N",
  *Replace* overwrites the selected kit with the current sounds, *Delete* (press twice) removes
  it. Tap a kit to load it on the current pattern (*Undo* reverts), or on every pattern with
  *All patterns* ticked. Kits are listed by number with their first sounds.
- **Panel** (as on the MatrixBrute): the current sound stays in place when you change pattern,
  and its settings are stored nowhere until you use *Edit* → *Store kit* (which writes it to the
  current pattern). Turning Panel off reloads the kit of the current pattern. The screen shows
  "PANEL" when it is active.
- **Edit**: pattern length, swing, per-track length (polymeter), BPM, Song playback, metronome,
  "Bake Grids" (writes the Grids rhythm to A–C).
- **Mute** / **Solo** (held + pads, or tapped to latch the mode): silence or isolate tracks;
  *Shift* + Mute/Solo clears all mutes/solos. To select a track without playing it: *Shift* +
  pad.
- **Holds with pads** (as on the DrumBrute): *Erase* + pad clears the track when stopped, or
  during playback erases its hits as the playhead passes while the pad is held; *Copy* + pad +
  pad copies the sequence of one track onto another.
- **Repeats while recording**: repeats are written into the pattern (one per step, or as
  ratchets if they are faster than the steps), like the DrumBrute Roller.
- **MIDI input** (PC: all ALSA ports, hot-pluggable, plus a virtual "Drumf(re)ake" port;
  Android: USB devices and MIDI apps):
  - one instrument = one channel: track A listens on channel 1, B on channel 2… H on channel 8,
    and plays the received notes at their pitch, with velocity (they are recorded in Rec);
  - otherwise (channels not assigned to any track, e.g. channel 10), the 8 notes of the major
    scale starting at the *Trig* note (C2 by default: white keys C2 to C3) trigger A–H at their
    own pitch;
  - settings in *Edit* → *MIDI settings*: channel of each track (can be off), *Trig* note,
    *Follow MIDI clock* (tempo, Start/Stop/Continue and Song Position from the MIDI clock,
    24 PPQN; the screen then shows "EXT" with the measured tempo);
  - the "MIDI" indicator on the screen blinks on every message; `--midi-monitor` prints the
    received messages in the terminal.
- The project is saved in `~/.local/share/drumfreake/project.json` and reloaded at startup.

## Architecture

```
src/core/    model (parameters, steps, patterns, factory sounds), JSON, Grids port
src/audio/   TrackVoice (Plaits + filter + drive + crush + choke), MasterFx, Engine (sequencer)
src/ui/      ImGui widgets and faceplate
shims/       minimal AVR headers to compile the grids/resources.cc tables
patches/     header overrides for the submodule (see patches/README.md)
```

Audio is rendered in blocks of 12 samples at 48 kHz; the sequencer is accurate to the block
(0.25 ms). The UI modifies the model under the engine lock; playback state, levels and the
oscilloscope go through atomics.

## Limitations

- The sampler is not emulated: the Sample button position is used as the Clouds Freeze button.
- The exact functions of the DrumFreak are not public: the faceplate follows the layout of the
  prototype photo, and the behavior of each button is an interpretation.

## License

Drumf(re)ake is distributed under the GNU GPL version 3 (see `LICENSE`).

Third-party code used:
- Plaits, Rings and stmlib (Émilie Gillet): MIT license;
- the Grids tables (`grids/resources.cc`): GPLv3.
