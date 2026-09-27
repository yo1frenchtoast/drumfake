// Hardware-looking widgets drawn with the ImGui draw list.
//
// All positions are given in "panel" coordinates (the 1540x850 layout of the
// prototype photo); Canvas converts them to screen pixels.

#ifndef DRUMFREAK_UI_WIDGETS_H_
#define DRUMFREAK_UI_WIDGETS_H_

#include "imgui.h"

namespace drumfreak {

const float kPanelWidth = 1540.0f;
const float kPanelHeight = 850.0f;

namespace colors {
const ImU32 kBackground = IM_COL32(24, 24, 26, 255);
const ImU32 kPanel = IM_COL32(52, 53, 56, 255);
const ImU32 kPanelEdge = IM_COL32(70, 71, 75, 255);
const ImU32 kButton = IM_COL32(30, 31, 33, 255);
const ImU32 kButtonTop = IM_COL32(42, 43, 46, 255);
const ImU32 kLabel = IM_COL32(200, 200, 200, 255);
const ImU32 kLabelDim = IM_COL32(130, 130, 134, 255);
const ImU32 kLedOff = IM_COL32(70, 70, 72, 255);
const ImU32 kLedWhite = IM_COL32(245, 245, 240, 255);
const ImU32 kOrange = IM_COL32(255, 106, 42, 255);
const ImU32 kRed = IM_COL32(240, 50, 40, 255);
const ImU32 kLockMod = IM_COL32(90, 200, 230, 255);  // LFO/macro modulation.
const ImU32 kPadFace = IM_COL32(228, 228, 226, 255);
const ImU32 kPadDark = IM_COL32(28, 28, 30, 255);
const ImU32 kScreen = IM_COL32(8, 12, 14, 255);
const ImU32 kScreenText = IM_COL32(210, 235, 240, 255);
const ImU32 kScreenDim = IM_COL32(90, 120, 128, 255);
const ImU32 kScreenAccent = IM_COL32(255, 140, 70, 255);
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
  KNOB_ORANGE,
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

// A rubber button. `face_text` is printed on the button, `label` below it.
// `led` draws a small LED in the top-left corner.
Interaction Button(const Canvas& c, const char* id, ImVec2 min, ImVec2 max,
                   const char* face_text, const char* label, Icon icon,
                   bool lit, ImU32 lit_color, bool led = false);

void DrawIcon(const Canvas& c, Icon icon, ImVec2 center, float size, ImU32 color);

}  // namespace drumfreak

#endif  // DRUMFREAK_UI_WIDGETS_H_
