# Drumfake

Open-source groovebox with 8 tracks, a step sequencer and performance controls. It runs on Linux, on Android tablets and in the browser, and is built on the open-source
[Mutable Instruments](https://github.com/pichenettes/eurorack) DSP:

- **Plaits**: one voice per track (8 tracks, 24 engines: analog and synthetic drums, modal,
  FM…);
- **Grids**: algorithmic rhythms on the X/Y pad;
- **Marbles**: random rhythms with a "deja vu" loop, written into the steps;
- **Peaks**: envelope and bouncing-ball modulation sources;
- **Rings**: reverb and chorus;
- **Clouds**: granular, stretch, looping delay and spectral effect on the master mix.

It also has a 64-step sequencer with parameter locks, polyrhythm, pattern chains, per-pattern
kits and a kit library, hold-button combinations, MIDI input with clock follow, and
full multitouch.

## Quick start

```sh
git clone --recursive https://github.com/yo1frenchtoast/drumfake
cd drumfake
cmake -B build && cmake --build build -j
./build/drumfake_tests
./build/drumfake
```

Press **Play**, select a track with its pad and press the steps. The project is saved with
`Ctrl+S` in `~/.local/share/drumfake/project.json`.

## Documentation

| Document | For |
|---|---|
| [User guide](docs/user-guide.md) | What the features do and how to use them |
| [Controls reference](docs/controls.md) | Every shortcut, gesture and hold combination |
| [Building](docs/building.md) | Desktop, Android and web builds, command line, troubleshooting |
| [Development guide](docs/development.md) | Code layout, threading rules, tests, how to extend |

## Limitations

- There is no sampler: the *Sample* button position is used as the Clouds *Freeze* button.

## License

Drumfake is distributed under the GNU GPL version 3 (see `LICENSE`).

Third-party code used:
- Plaits, Rings, Peaks, Marbles, Clouds and stmlib (Émilie Gillet): MIT license;
- the Grids tables (`grids/resources.cc`): GPLv3;
- SDL3, Dear ImGui, RtMidi and nlohmann-json: their own permissive licenses;
- Noto Sans (Android and web fonts): SIL OFL.
