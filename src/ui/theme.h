// Faceplate themes: every colour the panel is drawn with, and how the knobs
// and pads look. The screen stays a dark display in every theme; its accent
// follows the theme.

#ifndef DRUMFAKE_UI_THEME_H_
#define DRUMFAKE_UI_THEME_H_

#include <string>

#include "imgui.h"

namespace drumfake {

struct KnobColours {
  ImU32 body;
  ImU32 top;
  ImU32 pointer;
};

struct Theme {
  const char* id;    // Saved in the settings ("cream").
  const char* name;  // Shown ("Cream").

  // Panel.
  ImU32 background;
  ImU32 panel;
  ImU32 panel_edge;
  ImU32 bezel;
  ImU32 surface;
  ImU32 button;
  ImU32 button_top;
  ImU32 label;
  ImU32 label_dim;
  ImU32 led_off;
  ImU32 led_white;
  ImU32 accent;      // LEDs, locks, selection.
  ImU32 amber;       // Playhead, holds.
  ImU32 shift;       // Shift on: apart from the amber of Vary and Hold.
  ImU32 shift_face;  // Shift's rubber at rest.
  ImU32 red;         // Recording.
  ImU32 lock_mod;    // LFO / macro modulation.
  ImU32 xy_dots;     // The dot grid printed on the X/Y pad.
  ImU32 step_dim;    // Step numbers out of the pattern, last-step hints.
  ImU32 step_last;   // Steps up to the last one, with Last Step held.
  ImU32 step_on;     // A step that plays (when tracks have no colour of their own).
  ImU32 page_dim;    // Over the page buttons past the pattern's length.
  ImU32 knob_ring;   // The value ring of the edit knobs.
  ImU32 logo_light;  // The light half of the logo.

  // Pads.
  ImU32 pad_face;
  ImU32 pad_dark;
  ImU32 pad_dark_dim;  // Quiet words on the dark top of the track pads (the engine).
  ImU32 pad_ink;
  ImU32 pad_muted;
  ImU32 pad_label;   // What a lower pad does (chord, sound, slice...).
  ImU32 pad_meter;   // Level meter track on the track pads.
  // One colour per track, for the pads' faces and the steps when
  // `track_colours` is set; otherwise the accent everywhere.
  bool track_colours;
  ImU32 tracks[8];

  // Knobs, by KnobStyle (silver, black, amber, encoder).
  KnobColours knobs[4];

  // Screen (a dark display in every theme).
  ImU32 screen;
  ImU32 screen_text;
  ImU32 screen_dim;
  ImU32 screen_accent;
  ImU32 screen_on_accent;  // Words on the screen accent (toasts, selected items).
  ImU32 screen_rule;       // Value bars' tracks, outlines.
  ImU32 screen_frame;      // The scope's frame.
  ImU32 screen_tile;       // Unselected tiles (song, pattern lists).
  ImU32 screen_well;       // Backgrounds of lists.
  ImU32 screen_wave;       // Behind the slice editor's waveform.
  ImU32 screen_hover;      // A hovered row.
  // The screen's ImGui widgets (sliders, buttons, fields).
  ImVec4 widget_frame;
  ImVec4 widget_frame_hovered;
  ImVec4 widget_frame_active;
  ImVec4 widget_button;
  ImVec4 widget_button_hovered;
  ImVec4 widget_accent_light;  // A slider's grab while dragged.
};

enum ThemeId { THEME_MIDNIGHT, THEME_CREAM, THEME_SAGE, THEME_PLUM, THEME_FAKE, THEME_LAST };

// The theme the panel is drawn with.
const Theme& theme();
void SetTheme(int id);
int theme_index();
const Theme& theme_at(int id);
// THEME_LAST when no theme has that id ("classic", the Fake theme's old id, is
// still found).
int FindTheme(const std::string& id);
// ImGui's style for the screen's widgets, in the current theme: again after
// SetTheme.
void ApplyImGuiStyle();
// Lit colour of a track's pad and steps.
ImU32 TrackColour(int track);

}  // namespace drumfake

#endif  // DRUMFAKE_UI_THEME_H_
