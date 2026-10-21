# Drumf(re)ake

Open-source groovebox inspired by the Arturia DrumFreak prototype (not commercially released to
date) and the DrumBrute Impact workflow. Independent project, not affiliated with Arturia.
It runs on Linux, on Android tablets and in the browser, and is built on the open-source
[Mutable Instruments](https://github.com/pichenettes/eurorack) DSP:

- **Plaits**: one voice per track (8 tracks, 24 engines: analog and synthetic drums, modal,
  FM…);
- **Grids**: algorithmic rhythms on the X/Y pad;
- **Marbles**: random rhythms with a "deja vu" loop, written into the steps;
- **Peaks**: envelope and bouncing-ball modulation sources;
- **Rings**: reverb and chorus;
- **Clouds**: granular, stretch, looping delay and spectral effect on the master mix.

It also has a 64-step sequencer with parameter locks, polyrhythm, pattern chains, per-pattern
kits and a kit library, DrumBrute-style hold combinations, MIDI input with clock follow, and
full multitouch.

## Quick start

```sh
git clone --recursive https://github.com/yo1frenchtoast/drumfreake
cd drumfreake
cmake -B build && cmake --build build -j
./build/drumfreake_tests
./build/drumfreake
```

Press **Play**, select a track with its pad and press the steps. The project is saved with
`Ctrl+S` in `~/.local/share/drumfreake/project.json`.

## Documentation

| Document | For |
|---|---|
| [User guide](docs/user-guide.md) | What the features do and how to use them |
| [Controls reference](docs/controls.md) | Every shortcut, gesture and hold combination |
| [Building](docs/building.md) | Desktop, Android and web builds, command line, troubleshooting |
| [Development guide](docs/development.md) | Code layout, threading rules, tests, how to extend |

## Limitations

- There is no sampler: the *Sample* button position is used as the Clouds *Freeze* button.
- The exact functions of the DrumFreak are not public: the faceplate follows the layout of the
  prototype photo, and the behavior of each button is an interpretation.

## License

Drumf(re)ake is distributed under the GNU GPL version 3 (see `LICENSE`).

Third-party code used:
- Plaits, Rings, Peaks, Marbles, Clouds and stmlib (Émilie Gillet): MIT license;
- the Grids tables (`grids/resources.cc`): GPLv3;
- SDL3, Dear ImGui, RtMidi and nlohmann-json: their own permissive licenses;
- Noto Sans (Android and web fonts): SIL OFL.
