// Hardware-looking widgets drawn with the ImGui draw list.
//
// All positions are given in "panel" coordinates (the 1540x850 layout of the
// prototype photo); Canvas converts them to screen pixels.

#ifndef DRUMFREAKE_UI_WIDGETS_H_
#define DRUMFREAKE_UI_WIDGETS_H_

#include "imgui.h"

namespace drumfreake {

const float kPanelWidth = 1540.0f;
const float kPanelHeight = 815.0f;

// Slate panel, off-white pads, mint accent (LEDs, locks, selection), amber
// for the playhead and red for recording.
namespace colors {
const ImU32 kBackground = IM_COL32(18, 21, 24, 255);
const ImU32 kPanel = IM_COL32(43, 49, 56, 255);
const ImU32 kPanelEdge = IM_COL32(64, 73, 82, 255);
const ImU32 kBezel = IM_COL32(17, 20, 23, 255);
const ImU32 kSurface = IM_COL32(32, 37, 43, 255);
const ImU32 kButton = IM_COL32(20, 24, 28, 255);
const ImU32 kButtonTop = IM_COL32(34, 39, 46, 255);
const ImU32 kLabel = IM_COL32(201, 209, 214, 255);
const ImU32 kLabelDim = IM_COL32(126, 138, 147, 255);
const ImU32 kLedOff = IM_COL32(58, 67, 76, 255);
const ImU32 kLedWhite = IM_COL32(238, 242, 240, 255);
const ImU32 kAccent = IM_COL32(79, 209, 181, 255);
const ImU32 kAmber = IM_COL32(242, 184, 75, 255);
const ImU32 kRed = IM_COL32(229, 72, 77, 255);
const ImU32 kLockMod = IM_COL32(108, 166, 255, 255);  // LFO/macro modulation.
const ImU32 kPadFace = IM_COL32(233, 236, 230, 255);
const ImU32 kPadDark = IM_COL32(27, 31, 36, 255);
const ImU32 kScreen = IM_COL32(11, 16, 19, 255);
const ImU32 kScreenText = IM_COL32(214, 245, 238, 255);
const ImU32 kScreenDim = IM_COL32(111, 143, 138, 255);
const ImU32 kScreenAccent = kAccent;
}  // namespace colors

struct Canvas {
  ImDrawList* draw_list;
  ImVec2 origin;
  float scale;
  ImFont* font;
  ImFont* bold;

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
};

// Invisible hit area over a panel rectangle.
Interaction HitBox(const Canvas& c, const char* id, ImVec2 min, ImVec2 max);

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
  ICON_SAMPLE,
  ICON_BROWSE,
  ICON_EDIT,
  ICON_SAVE,
  ICON_BACK,
  ICON_SOUND,
  ICON_FILTER,
  ICON_TRACK,
  ICON_MASTER,
  ICON_MODS,
  ICON_ASSIGN,
  ICON_MUTE,
  ICON_KEYBOARD,
  ICON_SPICE,
  ICON_FX,
  ICON_LOOPER,
  ICON_XY,
};

// A rubber button. `face_text` is printed on the button (under the icon when
// there is one), `label` below it.
// `led` draws a small LED in the top-left corner.
Interaction Button(const Canvas& c, const char* id, ImVec2 min, ImVec2 max,
                   const char* face_text, const char* label, Icon icon,
                   bool lit, ImU32 lit_color, bool led = false);

void DrawIcon(const Canvas& c, Icon icon, ImVec2 center, float size, ImU32 color);

}  // namespace drumfreake

#endif  // DRUMFREAKE_UI_WIDGETS_H_
