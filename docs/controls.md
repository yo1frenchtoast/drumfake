# Controls reference

Drumf(re)ake is built around a faceplate that you drive with a mouse and keyboard on a desktop,
or with your fingers on a tablet. This page lists every gesture. For what the features do, see
the [user guide](user-guide.md).

## Faceplate, mouse and keyboard

| Faceplate | Mouse | Keyboard |
|---|---|---|
| Pads A–H | click = plays and selects the track (Shift = accent) | `Q W E R T Y U I` |
| Pads 1–8 | depends on the mode: Chords, Sounds, Repeats, Keys | `A S D F G H J K` |
| Knobs | drag vertically (Shift = fine), wheel, double-click = default; a bubble shows the name and the value while you hover or touch | |
| Perform: Matrix | click = shows the modulation matrix in place of the X/Y pad; click a cell to route a source, click the lit cell to clear it | |
| Steps 1–16 | click = on/off; right click = select the step (p-lock) | |
| Play / Rec | | `Space` / `Enter` |
| Pages Sound…Assign | | `1`–`8` |
| Step pages A–D | | `Z` / `X` |
| Last Step (held) / Poly / Shift (held) | | `L` / `P` / `Alt` |
| Save, Undo, Copy, Paste, Erase | | `Ctrl+S`, `Ctrl+Z`, `Ctrl+C`, `Ctrl+V`, `Delete` (acts on release) |
| Back (↺) | closes Edit, Browse and other screens; releases the selected step | `Esc` |

The eight pages, in order: **Sound**, **Filter**, **Track**, **Master**, **Mods**, **Envs**,
**Clouds**, **Assign**.

In Keys mode: `←` / `→` change the octave, `↑` / `↓` the root, `Tab` / `Shift+Tab` the scale.

## Touch

Everything is multitouch: several pads at once, a pad and a knob, the X/Y pad, and button
combinations (*Last Step* or *Copy* + pages, *Erase* + knob…). One free finger drives the
on-screen menus.

| Gesture | Replaces |
|---|---|
| Long press (0.5 s) | Right click: select a step, edit a chord on pads 1–8 |
| Double tap on a knob | Double-click: reset the knob to its default |

## Hold combinations

These work with the button held down, like on a DrumBrute.

| Hold | Then | Result |
|---|---|---|
| Shift | step 1–5 | Time division: 1/8, 1/8T, 1/16, 1/16T, 1/32 |
| Shift | step 13 / 14 / 15 / 16 | Metronome / At End / steps follow the playing page / Poly |
| Shift | pad | Select a track without playing it |
| Shift | Mute or Solo | Clear all mutes or solos |
| Last Step | step | Set the last step of the pattern (of the selected track in Poly) |
| Last Step | page A–D | Set the pattern length to 16, 32, 48 or 64 |
| Copy | source page, then destination page | Copy a page, or repeat it up to the destination |
| Copy | pad, then pad | Copy the sequence of one track onto another |
| Erase | pad | Clear the track (stopped), or erase its hits as the playhead passes (playing) |
| Erase | knob | Clear the locks of that parameter on the whole track |
| Mute / Solo | pads | Mute or isolate tracks (tap instead of hold to latch the mode) |
| A step (0.5 s, or right click) | knobs | Set parameter locks for that step |
| A step | pads 1–4 | Set its repeats (1 to 4 hits) |
| A step | Gate, Slide to locks (on the screen) | Give it its own gate length (up to 4 steps), or glide into its locks |
| A step | encoder | Shift it early or late (±50 % of a step, in 5 % increments) |

## Copy and Paste

Tap *Copy* (or `Ctrl+C`) to copy what is in front of you, then *Paste* (`Ctrl+V`) to paste it:

- with a step held: that step, locks included (paste it on another held step);
- in *Patterns* mode: the whole pattern, with its kit;
- otherwise: the sequence of the selected track (paste it on another selected track).

*Undo* reverts a paste.
