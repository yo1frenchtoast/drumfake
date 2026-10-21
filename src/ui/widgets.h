// Hardware-looking widgets drawn with the ImGui draw list.
//
// All positions are given in "panel" coordinates (a 1540x850
// layout); Canvas converts them to screen pixels.

#ifndef DRUMFAKE_UI_WIDGETS_H_
#define DRUMFAKE_UI_WIDGETS_H_

#include <unordered_map>
#include <vector>

#include "imgui.h"
#include "ui/theme.h"

namespace drumfake {

const float kPanelWidth = 1540.0f;
const float kPanelHeight = 815.0f;

// Colours come from the theme (ui/theme.h).

// Multitouch: controls that take their own fingers register a target each
// frame; the faceplate hit-tests new fingers against them.
enum TouchTargetKind {
  TOUCH_BUTTON,  // Any HitBox (buttons, steps).
  TOUCH_KNOB,
  TOUCH_TRACK_PAD,
  TOUCH_LOWER_PAD,
  TOUCH_XY,
};

struct TouchTarget {
  TouchTargetKind kind;
  int index;    // Pad number.
  ImGuiID id;   // Knob.
  ImVec2 min;   // Panel coordinates.
  ImVec2 max;
};

// Finger input pending for a knob.
struct KnobTouch {
  float delta = 0.0f;       // Upward drag, in panel units.
  bool double_tap = false;
  bool active = false;      // A finger is on the knob.
};

// Finger input pending for a button.
struct ButtonTouch {
  int fingers = 0;          // Fingers holding it.
  bool pressed = false;     // A finger landed since the last frame.
  bool released = false;    // A finger lifted since the last frame.
  double down_time = 0.0;
  bool long_press_done = false;
  int idle_frames = 0;      // Frames with no finger, before it is forgotten.
};

struct Canvas {
  ImDrawList* draw_list;
  ImVec2 origin;
  float scale;
  ImFont* font;
  ImFont* bold;
  std::vector<TouchTarget>* touch_targets = nullptr;
  std::unordered_map<ImGuiID, KnobTouch>* knob_touches = nullptr;
  std::unordered_map<ImGuiID, ButtonTouch>* button_touches = nullptr;

  ImVec2 P(float x, float y) const {
    return ImVec2(origin.x + x * scale, origin.y + y * scale);
  }
  ImVec2 P(ImVec2 p) const { return P(p.x, p.y); }
  float S(float v) const { return v * scale; }

  // Text centered on (x, y) (panel coordinates), or left-aligned.
  void Text(float x, float y, float size, ImU32 color, const char* text,
            bool centered = true, bool use_bold = false) const;
};

struct Interaction {
  bool hovered = false;
  bool active = false;         // Held down (left button).
  bool pressed = false;        // Went down this frame.
  bool released = false;       // Went up this frame.
  bool clicked = false;
  bool right_clicked = false;
  bool double_clicked = false;
  bool touch = false;          // Driven by a finger rather than a mouse.
  bool long_pressed = false;   // Finger held still for 0.5 s (this frame).
  bool touch_active = false;   // Knob turned by a finger.
};

// Invisible hit area over a panel rectangle. With `touch_button`, fingers can
// hold it too (each finger independently); controls that handle their own
// fingers (pads, knobs, XY pad) pass false.
Interaction HitBox(const Canvas& c, const char* id, ImVec2 min, ImVec2 max,
                   bool touch_button = true);

enum KnobStyle {
  KNOB_SILVER,
  KNOB_BLACK,
  KNOB_AMBER,
  KNOB_ENCODER,
};

// Optional overlays showing what the sequencer does to a parameter.
struct KnobDecor {
  ImU32 halo = 0;          // Glow around the knob (a lock is playing).
  float marker = -1.0f;    // Effective value, drawn on the ring when >= 0.
  ImU32 marker_color = 0;
  bool automated = false;  // Some step of the track locks this parameter.
  ImU32 badge_color = 0;   // Color of that badge (default: accent).
};

// Returns true when the value changed. `ring` draws the value arc around the
// knob (Sound Edit knobs); `ring_color` highlights parameter locks.
bool Knob(const Canvas& c, const char* id, ImVec2 center, float radius,
          float* value, float default_value, KnobStyle style, bool ring,
          ImU32 ring_color, Interaction* interaction = nullptr,
          const KnobDecor* decor = nullptr);

enum Icon {
  ICON_NONE,
  ICON_RECORD,
  ICON_STOP,
  ICON_PLAY,
  ICON_SONG,
  ICON_FREEZE,
  ICON_BROWSE,
  ICON_EDIT,
  ICON_SAVE,
  ICON_BACK,
  ICON_SOUND,
  ICON_FILTER,
  ICON_TRACK,
  ICON_MASTER,
  ICON_MODS,
  ICON_ENVS,
  ICON_CLOUDS,
  ICON_ASSIGN,
  ICON_MUTE,
  ICON_KEYBOARD,
  ICON_GRIDS,
  ICON_FX,
  ICON_LOOPER,
  ICON_XY,
};

// A rubber button. `face_text` is printed on the button (under the icon when
// there is one), `label` below it.
// `led` draws a small LED in the top-left corner. `face_color` (0 = the usual
// one) sets the rubber's color when it is not lit.
Interaction Button(const Canvas& c, const char* id, ImVec2 min, ImVec2 max,
                   const char* face_text, const char* label, Icon icon,
                   bool lit, ImU32 lit_color, bool led = false, ImU32 face_color = 0);

void DrawIcon(const Canvas& c, Icon icon, ImVec2 center, float size, ImU32 color);

}  // namespace drumfake

#endif  // DRUMFAKE_UI_WIDGETS_H_
