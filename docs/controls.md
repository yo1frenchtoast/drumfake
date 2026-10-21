# Controls reference

Drumfake is built around a faceplate that you drive with a mouse and keyboard on a desktop,
or with your fingers on a tablet. This page lists every gesture. For what the features do, see
the [user guide](user-guide.md).

## Faceplate, mouse and keyboard

| Faceplate | Mouse | Keyboard |
|---|---|---|
| Pads A–H | click = plays and selects the track (Shift = accent); right click = selects it without playing | `Q W E R T Y U I` |
| Pads 1–8 | depends on the mode: Chords (chords of the scale), Sounds, Repeats, Keys | `A S D F G H J K` |
| Knobs | drag vertically (Shift = fine), wheel, double-click = default; a bubble shows the name and the value while you hover or touch | |
| Perform: X/Y pad | drag = moves the point; right click = holds FX, Looper or Macros at that point, right click again to release | |
| Perform: Shift + Spice | shows Vary on the pad (drag left/right = amount); Shift + Vary = new variations (Dice); Spice = back to Grids | `Alt` + click |
| Perform: Matrix | click = shows the modulation matrix in place of the X/Y pad; click a cell to route a source, click the lit cell to clear it | |
| Steps 1–16 | click = on/off; right click = select the step (p-lock) | |
| Play / Rec | | `Space` / `Enter` |
| Pages Sound…Assign | | `1`–`8` |
| Step pages A–D | | `Z` / `X` |
| Last Step (held) / Poly / Shift (held) | right click or long press on Shift or Last Step locks it until a combination is used (or the button is pressed again) | `L` / `P` / `Alt` |
| Save, Undo, Copy, Paste, Erase | | `Ctrl+S`, `Ctrl+Z`, `Ctrl+C`, `Ctrl+V`, `Delete` (acts on release) |
| Edit: Theme | switches the look (Cream, Midnight, Sage, Plum, Classic), saved for the machine | |
| Back (↺) | closes Edit, Browse and other screens; releases the selected step | `Esc` |
| WAV file dropped on the window | imports it onto the pad under it (A–H), or the selected track | |
| Browse: Sounds / Samples | the factory sounds, or the project's sample pool; tap one (or turn the encoder) to load it on the selected track; Samples: *Slices 4/8/16/32/Auto/Clear* slice the track's sample, *Sens* sets how many attacks Auto keeps, *Edit* opens the slice editor | |
| Slice editor | click: select (and hear) a slice; drag a start: move it; double-click: add a start; right click / long press on a start: remove it; wheel: zoom | encoder: nudge the selected start |
| Shift + Freeze | resample one loop of the pattern into a new sample (again: cancel) | |
| Shift + Keys | Slices mode: pads 1–8 play the slices of the track's sample; Shift + Slices: next 8; Keys: back to the keyboard | |

The eight pages, in order: **Sound**, **Filter**, **Track**, **Master**, **Mods**, **Envs**,
**Clouds**, **Assign**.

In Keys and Chords modes: `←` / `→` change the octave, `↑` / `↓` the root, `Tab` / `Shift+Tab` the scale.
In Chords mode the pads A–H choose the kind of chord (Triad, 7th, Sus2, Sus4, Add9, 6th, 9th, Power).
The pads A–H (`Q W E R T Y U I`) play the black keys, G and H are Oct − / Oct +; with `Shift` they
choose the scale, and `Shift` + a pad 1–8 sets the root. Right click a pad A–H to select the track.

## Touch

Everything is multitouch: several pads at once, a pad and a knob, the X/Y pad, and button
combinations (*Last Step* or *Copy* + pages, *Erase* + knob…). One free finger drives the
on-screen menus.

| Gesture | Replaces |
|---|---|
| Long press (0.5 s) | Right click: select a step |
| Double tap on a knob | Double-click: reset the knob to its default |
| Finger on the X/Y pad + lit mode button (FX, Looper, Macros) | Right click on the pad: hold it there (the lit button again releases it) |

## Hold combinations

These work with the button held down.

| Hold | Then | Result |
|---|---|---|
| Shift | step 1–5 | Time division: 1/8, 1/8T, 1/16, 1/16T, 1/32 |
| Shift | step 13 / 14 / 15 / 16 | Metronome / At End / steps follow the playing page / Poly |
| Shift | pad A–H | Select a track without playing it (in Keys mode: choose the scale; Shift + pad 1–8 sets the root) |
| Shift (or right click) | Mute or Solo | Clear all mutes or solos |
| Shift | Panel (or right click, long press) | Arm the snapshot of all sounds and settings; while it is armed, a plain press of Panel restores it |
| Last Step | step | Set the last step of the pattern (of the selected track in Poly) |
| Last Step | page A–D | Set the pattern length to 16, 32, 48 or 64 |
| Shift | Steps | The steps follow the playing page again (after a page was chosen by hand) |
| Shift | page A–D | Extend to 16, 32, 48 or 64 steps by repeating the pattern (Poly: the selected track in Steps view, the whole pattern in Patterns view) |
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
