# User guide

Drumfake is a groovebox with 8 tracks. Each track has its own sound (a Plaits voice), a
filter, sends to the master effects, and a sequence of up to 64 steps. This guide explains the
features in the order you meet them. The exact gestures are in the
[controls reference](controls.md).

## 1. The basics

**Tracks and pads.** The big pads A–H are the 8 tracks. Touching one plays it and selects it:
the knobs, the screen and the steps now show that track. The smaller pads 1–8 have several
modes (see [section 6](#6-performing)).

**The screen and the knobs.** The 8 knobs under *Sound Edit* show the parameters of the current
page. The page buttons select the page: **Sound**, **Filter**, **Track**, **Master**, **Mods**,
**Envs**, **Clouds**, **Assign**. The screen shows the 8 values, the playing pattern, the tempo
and a scope. The round encoder changes the Plaits engine of the track (or the sound in Browse).

**First groove.** Press **Play**. Select a track, then press steps 1–16 to turn them on or
off. The demo pattern is a good start: *Edit* → *Demo pattern*.

**Saving.** *Save* (or `Ctrl+S`) writes the project to `~/.local/share/drumfake/project.json`
(the app's private storage on Android). It is reloaded at startup. *Undo* reverts the last
edit of the current pattern, up to 32 times.

## 2. Sequencing

### Steps and pages

A pattern has up to 64 steps, shown 16 at a time: pages **A|16**, **B|32**, **C|48**, **D|64**.
During playback the steps follow the page played by the selected track. Choosing a page by
hand freezes the display until the next Play, or until *Shift* + *Steps*, which makes the steps
follow the playing page again (and turns Follow back on if it was off).

- **Length.** *Edit* → *Length* sets the pattern length. *Last Step* held, then a step or a
  page, sets it without opening Edit.
- **Extending.** *Shift* + a page sets the length to 16, 32, 48 or 64 steps and, when it grows,
  repeats what is already there (locks included): a 16-step pattern and
  *Shift* + `B|32` gives the same 16 steps twice. A length that is not a whole page repeats on its
  own period (12 steps to 48 gives four times the 12). A shorter length just shortens.
  With **Poly** on, the *Steps* view extends the selected track alone (it gets its own length),
  and the *Patterns* view the whole pattern: tracks with their own length keep it. Undo restores
  the pattern.
- **Copying pages.** *Copy* held, then a source page, then a destination page. `A|16` then
  `D|64` repeats the first 16 steps (locks included, all tracks) up to step 64 and extends the
  pattern. A destination before the source copies the page.
- **Notes.** The *Notes* button makes the steps show their note offset (in semitones, relative
  to the track pitch) instead of their number.

### Patterns and songs

There are 16 patterns. Press **Patterns**, then a step button to choose one. While playing, the
change waits for the end of the current pattern (*At End*, toggled with *Shift* + step 14),
and the queued pattern blinks amber.

**Song** chains patterns. Press *Song* (it blinks), then step buttons 1–16 in the order you
want. A pattern can come back (1-1-2-3), up to 64 entries. The first press starts a new chain,
*Undo* removes the last entry, *Erase* clears the chain. Press *Song* again to play the chain in
a loop (from the start on Play; while playing, after the current pattern). The chain is saved
with the project.

### Step details

Hold a step for half a second (or right click it) to select it. While it is held:

- the screen shows its **velocity**, **probability**, **ratchets**, **note**, **nudge**,
  **gate** and **slide**;
- turning a knob sets a **parameter lock**: that step plays with its own value for that
  parameter (see below);
- pads 1–4 set its repeats (1 to 4 hits);
- the encoder shifts it in time, early or late.

**Parameter locks.** A locked knob has an orange ring. Double-click (or double-tap) the knob to
remove the lock. While a locked step plays, the knob has an orange halo. An orange dot shows
that some step of the track locks that parameter. To remove the locks of a parameter on the
whole track, hold *Erase* and touch the knob.

**Gate.** By default a note lasts as long as the *Gate* knob of the Filter page says (1 ms to
about half a second). A step can have its own gate instead, from a fraction of a step up to 4
steps (with ratchets, a fraction of one hit). Past one step the note is tied over the next
steps. The step shows a bar for it: white up to one step, amber beyond.

**Slide.** With *Slide to locks* on, the parameters do not jump to the step's locks: they glide
there linearly from their previous values during the step (a small line in the corner of the
step shows it). A slide step without locks glides back to the knobs. The choices (engine,
filter mode, choke group) always jump. Use it for filter sweeps and pitch glides between steps.

**Motion recording.** Press **Rec** and **Play**, then turn a knob: the value is written as a
lock on the step that is playing.

**Live recording.** With **Rec** on and the sequencer playing, the pads record their hits into
the pattern.

### Timing

- **Swing**: *Edit* → *Swing*, per pattern.
- **Locking Shift and Last Step**: a right click (or a long press) on *Shift* or *Last Step* keeps it
  on without holding it. It releases after the next combination (for Shift a step, a pad, Mute, Solo
  or Panel; for Last Step a step or a page), or when you press the button again.
- **Time division**: *Shift* + step 1–5 sets the step length (1/8, 1/8T, 1/16, 1/16T, 1/32).
- **Per-track length and polyrhythm.** Give a track its own length in *Edit*, or use *Last
  Step* with a track selected. With **Poly** on (per pattern), each track loops on its own
  length without restarting at the end of the pattern; everything realigns on Play and on a
  pattern change. Without Poly, short tracks restart with the pattern.
- **Tempo**: the *Tempo* knob, *TAP*, or *Edit* → *BPM*. With *Follow MIDI clock* on, the tempo
  comes from the outside (the screen shows `EXT`).
- **Vary**: *Shift* + *Spice*, per pattern (see [the X/Y pad](#the-xy-pad)). It changes the
  pattern as it plays, without writing
  anything into the steps: hits get softer, some are dropped, soft ghost notes (an echo of the
  previous hit of the track) appear on empty steps, the timing loosens a little, a few hits
  become double ratchets, and decays get shorter. The amount sets how much of each, from off
  to the full table below. The variations are drawn once and repeat on every loop, so the groove
  stays a groove; *Shift* + *Vary* (the Dice) draws new ones. The amount and the draw are saved
  with the pattern.

  | At 100% | |
  |---|---|
  | Velocity | down to 40% of the step's |
  | Dropped hits | up to 30% of them |
  | Ghost notes | on up to 15% of the empty steps (not on an empty track), at 25–45% of the velocity |
  | Timing | ±8% of a step |
  | Ratchets | up to 12% of the single hits play twice |
  | Decay | down to half (the step's lock or the knob) |

## 3. Sound design

### Sound, Filter and Track pages

| Page | Parameters |
|---|---|
| **Sound** | Engine, Pitch, Harmonics, Timbre, Morph, Decay, LPG Colour, Out/Aux (the Plaits controls). |
| **Filter** | Cutoff, Resonance, Mode (low, band, high pass), Filter Env amount and decay, Drive, Crush, Gate length. |
| **Track** | Volume, Pan, Reverb and Delay sends, Pitch / Timbre / Morph envelope amounts, Choke group. |

The first 24 engines are those of Plaits; the last one, **Sample**, plays a sample (see
[Sampler](#sampler)). Tracks in the same **Choke group** cut each other off (for an open and a
closed hi-hat).

### Sampler

Any track can play a sample instead of a Plaits voice. It keeps everything else: the filter,
drive and crusher, the sends, the choke group, modulations, locks, Chords, Keys and Vary.

- **Importing.** Drop WAV files on the window: on a pad A–H, the file goes to that track;
  anywhere else, to the selected track. The file is copied into `samples/` beside the project,
  takes a slot of the project's **pool** (128 slots, saved with the project) and the track
  switches to the **Sample** engine, set to play the whole sample once at its own pitch.
  Dropping the same file again reuses its slot.
- **Files.** WAV, 8 to 32-bit or float, mono or stereo (mixed to mono), 8 to 192 kHz
  (converted to 48 kHz), up to 60 seconds (longer files are cut, the message says so).
- **Browse → Samples** lists the pool: tap a sample (or turn the encoder) to put it on the
  selected track. A track already on the Sample engine keeps its settings and changes sample.
- **The Sound page on the Sample engine:**

  | Knob | On the Sample engine |
  |---|---|
  | Pitch | C4 plays the sample at its own speed; each semitone up plays it faster. |
  | Start (Harm) | Where it starts, from the beginning (0%) to the end (100%). |
  | Len (Timb) | How much of it plays after Start, up to all of it; below the middle (*rev*), it plays **backwards** from Start. The middle plays nothing. |
  | Loop (Mrph) | *Off* plays once. Above, the part from this point to the end of Len loops while the note lasts (with a short crossfade): the higher, the shorter the loop. |
  | Rel (Dcy) | Release after the gate closes (Gate on the Filter page). *Full* plays the sample to its end. |
  | Smpl (Aux) | The slot of the pool. |

  The screen shows the sample (in place of the scope) with **Start** (green), the end of **Len**
  (red) and the **Loop** point (amber); while playing, they follow locks and modulations.
- **Per step**: every one of these knobs can be locked on a step, including **Smpl**, so one
  track can play a different sample on each step (a whole kit on one track), and a modulation
  can move Start or change the sample.
- A sample that cannot be loaded (file missing from `samples/`) leaves its slot silent and is
  reported when the project opens; the rest of the project loads.

### Modulation

Every parameter of the Sound, Filter and Track pages can be a modulation destination. On each
page below, *Dest* chooses the parameter (*Off* by default) and *Amount* or *Depth* sets how
far it goes: the middle of the knob means no modulation, the two sides go up or down.
Modulations are per track, and saved in the kit.

- **Mods page: two LFOs.** Rate (from slow to fast, synced to the tempo), shape (sine,
  triangle, saw up, saw down, square, sample and hold), depth, destination.
- **Envs page: two sources from Mutable Instruments Peaks**, restarted by every note:
  - an **AD envelope**: attack, decay, amount, destination. Good for a pitch drop on a kick or a
    filter that opens on each hit;
  - a **bouncing ball**: gravity, bounce, amount, destination. The parameter falls like a
    bouncing ball. Gravity sets how fast, bounce how elastic (a high value makes it bounce
    for longer). It gives rolls that speed up or slow down when it drives a volume or a
    cutoff.
- **Macros.** With the **Macros** mode of the X/Y pad, the pad moves two parameters of the
  selected track at once (chosen on the *Assign* page).
- **Modulation matrix.** The **Matrix** button, after *Macros*, swaps the X/Y pad for a grid: one
  row per source (LFO 1, LFO 2, Env, Ball), one column per parameter, on three tabs (Sound,
  Filter, Track). Tap a cell to send the source there, tap the lit cell to turn it off. The fill
  of the cell shows the amount (blue-green up, amber down), which you set with the knobs of the
  Mods and Envs pages. A dot on a tab means a source points to that page. A source has a single
  destination, so a row lights at most one cell.

### Kits and sounds

A **kit** is the 8 sounds of a pattern (engine, all parameters, modulation settings).

- Each pattern has **its own kit**, loaded with it, also in Song mode. The knobs always edit
  the kit of the current pattern. *Copy* and *Paste* of a pattern also copy its kit.
- **Browse** lists the 16 factory sounds. Tap one to load it on the selected track. In *Sounds*
  mode, pads 1–8 do the same (8 sounds per bank; tap *Sounds* again for bank 2).
- **Panel mode** (as on the MatrixBrute): the current sound stays as it is when you change
  pattern. Your edits are stored nowhere until you use *Edit* → *Store kit*, which writes them
  to the current pattern. Turning Panel off reloads the kit of the current pattern. The screen
  shows `PANEL` when active.
- **Snapshot** (for a drop): **Shift + Panel**, a right click or a long press on *Panel* arms a
  snapshot of everything you can play with: the sounds of the 8 tracks, the Master, Clouds and
  Assign settings, tempo and volume, **mute and solo**, the Grids position and the Keys and
  Chords settings. Turn knobs, mute, change pattern, change tempo, as much as you like (as in
  Panel mode, none of it reaches the patterns), then press *Panel*: everything comes back to
  what it was when you armed it (the shortcut gestures work too). The button blinks and the
  screen shows `SNAPSHOT` while it is armed. The steps of the patterns are
  not part of it. If you changed pattern in the meantime, the sound of the pattern you are on
  comes back (its own kit), the rest returns to the snapshot.
- **Kit library**: *Edit* → *Kit library...*. Kits saved as files, shared by all projects
  (`kits/` beside `project.json`):
  - *Save new* stores the current 8 sounds as the next "Kit N";
  - tap a kit to load it on the current pattern (*Undo* reverts), or on every pattern with
    *All patterns* ticked;
  - *Replace* overwrites the selected kit with the current sounds;
  - *Delete* (press twice) removes it.

## 4. Generating rhythms

### Grids (X/Y pad, "Spice")

The X/Y pad in **Spice** mode moves through the Grids drum map: the position picks the rhythm of
three parts, played on tracks A–C. Click *Spice* again to turn it on or off. The *Assign* page
sets the density of each part and the chaos. *Edit* → **Bake Grids** writes the current Grids
rhythm into the steps of tracks A–C, so you can edit it.

### Random rhythm (Marbles)

*Edit* → **Random rhythm...** writes a random rhythm into two tracks, using the gate generator
of Mutable Instruments Marbles.

| Setting | Meaning |
|---|---|
| Model (first slider) | Complementary, Clusters, Drums, Independent, Divider, Three states, Markov: how the two outputs relate to each other. *Drums* draws from 18 kick and snare patterns; *Markov* favours sparse patterns that echo themselves. |
| Bias | Density or pattern choice, depending on the model. |
| Deja vu | 0 %: every draw is new. 100 %: a loop of *Loop* steps that repeats. In between, the loop slowly changes. |
| Loop | Length of the deja vu loop, 1 to 16 steps. |
| t1, t3 | The tracks that receive the two outputs (A and B by default). |

Press **Generate**: the steps of both tracks are replaced over the pattern length. *Undo*
reverts it. Press it again for another draw. It is a good way to find a starting point: turn
deja vu up to 100 %, generate until you like the loop, then edit the steps by hand.

## 5. Master section

- **Master page**: reverb (size, tone, level), a tempo-synced ping-pong **delay** (time,
  feedback, level), **chorus**, drive. The reverb and chorus come from Mutable Instruments
  Rings. Each track sends to them with its *Rev* and *Dly* knobs.
- **Clouds page**: Mutable Instruments Clouds on the whole mix. It is off until **D/W**
  (dry/wet) is raised.

  | Mode | What it does |
  |---|---|
  | Granular | Plays grains of the last ~2 seconds. *Dens* sets the number of grains: at 50 % there is none, and the further you go either way the denser it gets (below 50 %, the grains repeat exactly). |
  | Stretch | Plays the recording slower or faster without changing its pitch. |
  | Looping | A looping delay, with a pitch shifter. |
  | Spectral | Transforms the spectrum of the sound. |

  The other knobs are *Pos* (where in the recording to read from), *Size* (grain or window
  size), *Pitch* (±24 semitones), *Text* (texture: filters, diffusion, window shape, depending
  on the mode), and *Fdbk* (feedback). The **Freeze** button (top row, left of the LEDs) stops
  the recording and loops what it holds; press it again to release. Clouds runs at 48 kHz, so
  its times are two thirds of those of the module.
- **Volume** and **Tempo** are the two knobs at the top left.

## 6. Performing

### The X/Y pad

Four modes, chosen with the buttons above the pad (the fifth button, **Matrix**, replaces the
pad with the [modulation matrix](#modulation)):

| Mode | Effect while you touch the pad |
|---|---|
| **Spice** | Moves through the Grids drum map (see above). |
| **FX** | X = DJ filter (low pass on the left, high pass on the right), Y = delay throw. |
| **Looper** | Loops the sequencer for as long as you touch the pad. Y sets the length. |
| **Macros** | Moves the two parameters assigned on the *Assign* page. |

**Vary.** *Shift* + *Spice* turns the pad into the Vary slider of the current pattern: the
*Spice* button reads *Vary* (amber LED), and sliding left or right sets the amount (the left edge
turns it off). *Shift* + *Vary* draws new variations; *Spice* alone goes back to Grids, and any
other mode leaves it. A locked Shift works too.

**Hold.** FX, Looper and Macros only act while you touch the pad. To keep one going, press its
lit mode button while a finger is on the pad (with a mouse: right click the pad where you want
it). The point turns amber and *HOLD* shows on the pad: it stays there after you let go, and
touching the pad moves it. Press the lit button again (or right click the pad) to release it;
changing mode or opening the Matrix also releases it. A held Looper keeps looping.

### Pads 1–8

| Mode | Behaviour |
|---|---|
| **Chords** | Plays real chords on the selected track, built on the scale of the Keys settings (see below). |
| **Sounds** | Loads factory sounds on the selected track. |
| **Repeats** | Repeats the held tracks at the chosen division. With Rec on, the repeats are written into the pattern (one per step, or as ratchets when faster than a step). |
| **Keys** | Plays the selected track on a scale and writes the note into the selected step. Pads 1–8 are the notes of the scale, the instrument pads A–H are the black keys (see below). |

**Chords.** In *Chords* mode, pads 1–8 play a chord on the selected track: the chord built on
each note of the scale (in C major: C, Dm, Em, F, G, Am, B°, C), named on the pad. The pads A–H
choose the kind of chord: **Triad**, **7th**, **Sus2**, **Sus4**, **Add9**, **6th**, **9th** and
**Power** (root and fifth); the chosen one is lit. Chords use the root, scale and octave of
the Keys settings (press *Chords* again for that page, *Shift* works as in Keys mode: Shift + A–H
the scale, Shift + 1–8 the root). The notes play at fixed pitches, on the sound of the track.
They are played by a shared pool of four extra voices: a chord can have up to five notes, and
when the voices are all busy the oldest note is cut. With a step held, a chord is written into
that step (the pad of the step shows dots, one per note), and with **Rec** and **Play** into the
step that is playing; playing a single note on the step replaces the chord. A step chord plays
with its gate, locks and probability like any note.

**Layout** (like the BeatStep Pro). Pads 1–8 are the white keys, the instrument pads A–H sit
above them like the black keys: A and B are the C♯ and D♯ keys, there is no key above the third
pad (E–F), then D, E and F are F♯, G♯ and A♯. **G** and **H** are *Oct −* and *Oct +*. Each pad
shows its note.

- **Scale.** Hold *Shift* and press A–H: Chromatic, Major, Minor, Dorian, Mixolydian, Harmonic
  Minor, Blues, Pentatonic Minor. The default, **Chromatic**, is the piano. With the other scales,
  pads 1–8 are the degrees of the scale and the black keys sit a semitone above each degree.
- **Root (transposition).** Hold *Shift* and press a pad 1–8: that key becomes the root, and the
  whole keyboard moves with it (root D: D E F♯ G A B C♯ D).
- A right click on a pad A–H selects that track without playing, since the pads are keys here.

**Keys settings**: root, scale (major, minor, modes, harmonic minor, pentatonics, blues,
chromatic) and octave, shared by the whole project. The *Keys* button shows a dedicated page
(knobs 1–3 set root, scale and octave; press *Keys* again to come back); the screen shows the
pad notes. The keyboard has fixed pitches, whatever the Pitch knob of the track: with root C and
octave 0, pad 1 is C3 and the pads follow a standard piano. Changing the root transposes all the
keys together (root D: D E F♯ G A B C♯ D), the octave moves them by 12 semitones.

### Mute and Solo

Hold **Mute** or **Solo** and touch pads to mute or isolate tracks, or tap the button to latch
the mode. *Shift* + Mute or Solo, or a right click on the button, clears them all.

## 7. MIDI

**Input.** On a PC, every ALSA port is available and hot-pluggable, plus a virtual port called
*Drumfake*. On Android, USB devices and MIDI apps are available. The **MIDI** indicator on
the screen blinks on each message; `--midi-monitor` prints them in the terminal.

- **One instrument = one channel.** Track A listens on channel 1, B on channel 2… H on channel
  8, and plays the notes it receives at their pitch and velocity. With Rec on, they are
  recorded.
- **Other channels** (channels assigned to no track, for example channel 10): the 8 notes of
  the major scale starting at the *Trig* note (C2 by default, so the white keys C2 to C3)
  trigger tracks A–H, each at its own pitch.
- **Settings**: *Edit* → *MIDI settings*: the channel of each track (or off), the *Trig* note,
  and **Follow MIDI clock**: tempo, Start, Stop, Continue and Song Position come from the MIDI
  clock (24 PPQN).

There is no MIDI output yet.

## 8. Offline rendering

```sh
./build/drumfake --render demo.wav --bars 4 --pattern 1
```

renders a pattern to a WAV file without opening a window (see [building](building.md) for all
the options).

## 9. Limits

- The *Sample* button position is used as the Clouds *Freeze* button; the sampler is the
  **Sample** engine. Samples can be imported by dropping files on the desktop only for now.
- The pool belongs to the project: a kit of the kit library that uses samples refers to slots of
  the pool, not to files.
