#include "ui/theme.h"

namespace drumfake {

namespace {

// Cream: a cream panel, warm dark-brown print and terracotta accents, like
// vintage studio gear; bakelite knobs; each track its own colour.
const Theme kCream = {
  "cream", "Cream",
  IM_COL32(52, 44, 38, 255),     // background
  IM_COL32(234, 226, 208, 255),  // panel
  IM_COL32(196, 182, 156, 255),  // panel_edge
  IM_COL32(44, 37, 32, 255),     // bezel
  IM_COL32(222, 211, 189, 255),  // surface
  IM_COL32(150, 136, 114, 255),  // button
  IM_COL32(214, 202, 179, 255),  // button_top
  IM_COL32(62, 48, 38, 255),     // label
  IM_COL32(128, 110, 94, 255),   // label_dim
  IM_COL32(176, 162, 140, 255),  // led_off
  IM_COL32(255, 252, 244, 255),  // led_white
  IM_COL32(196, 92, 56, 255),    // accent: terracotta
  IM_COL32(214, 156, 52, 255),   // amber: mustard
  IM_COL32(150, 70, 36, 255),    // shift: rust
  IM_COL32(194, 166, 136, 255),  // shift_face
  IM_COL32(186, 48, 44, 255),    // red
  IM_COL32(70, 112, 164, 255),   // lock_mod
  IM_COL32(150, 132, 110, 120),  // xy_dots
  IM_COL32(146, 130, 112, 255),  // step_dim
  IM_COL32(112, 96, 82, 255),    // step_last
  IM_COL32(255, 252, 244, 255),  // step_on (unused: tracks have colours)
  IM_COL32(120, 104, 86, 70),    // page_dim
  IM_COL32(62, 48, 38, 255),     // knob_ring
  IM_COL32(62, 48, 38, 255),     // logo_light
  IM_COL32(248, 243, 233, 255),  // pad_face: ivory
  IM_COL32(52, 43, 36, 255),     // pad_dark
  IM_COL32(64, 50, 40, 255),     // pad_ink
  IM_COL32(176, 166, 152, 255),  // pad_muted
  IM_COL32(132, 114, 98, 255),   // pad_label
  IM_COL32(84, 72, 62, 255),     // pad_meter
  true,
  {
    IM_COL32(196, 92, 56, 255),    // terracotta
    IM_COL32(214, 160, 60, 255),   // mustard
    IM_COL32(128, 140, 70, 255),   // olive
    IM_COL32(62, 132, 126, 255),   // teal
    IM_COL32(84, 110, 156, 255),   // slate blue
    IM_COL32(136, 84, 124, 255),   // plum
    IM_COL32(204, 112, 112, 255),  // rose
    IM_COL32(140, 102, 70, 255),   // walnut
  },
  {
    { IM_COL32(38, 32, 28, 255), IM_COL32(66, 56, 48, 255), IM_COL32(246, 236, 218, 255) },    // silver
    { IM_COL32(30, 25, 22, 255), IM_COL32(56, 47, 40, 255), IM_COL32(246, 236, 218, 255) },    // black
    { IM_COL32(172, 78, 46, 255), IM_COL32(204, 98, 62, 255), IM_COL32(252, 238, 222, 255) },  // amber
    { IM_COL32(38, 32, 28, 255), IM_COL32(214, 204, 186, 255), IM_COL32(90, 80, 70, 255) },    // encoder
  },
  IM_COL32(24, 21, 18, 255),     // screen
  IM_COL32(244, 232, 210, 255),  // screen_text
  IM_COL32(158, 140, 118, 255),  // screen_dim
  IM_COL32(232, 136, 90, 255),   // screen_accent
  IM_COL32(40, 20, 10, 255),     // screen_on_accent
  IM_COL32(70, 58, 48, 255),     // screen_rule
  IM_COL32(56, 46, 38, 255),     // screen_frame
  IM_COL32(58, 47, 39, 255),     // screen_tile
  IM_COL32(40, 33, 28, 255),     // screen_well
  IM_COL32(32, 27, 23, 255),     // screen_wave
  IM_COL32(110, 86, 68, 120),    // screen_hover
  ImVec4(0.17f, 0.14f, 0.12f, 1.0f),
  ImVec4(0.23f, 0.18f, 0.15f, 1.0f),
  ImVec4(0.29f, 0.22f, 0.18f, 1.0f),
  ImVec4(0.23f, 0.18f, 0.15f, 1.0f),
  ImVec4(0.31f, 0.24f, 0.20f, 1.0f),
  ImVec4(0.96f, 0.68f, 0.52f, 1.0f),
};

// Classic: the original slate panel with its mint accent.
const Theme kClassic = {
  "classic", "Classic",
  IM_COL32(18, 21, 24, 255),
  IM_COL32(43, 49, 56, 255),
  IM_COL32(64, 73, 82, 255),
  IM_COL32(17, 20, 23, 255),
  IM_COL32(32, 37, 43, 255),
  IM_COL32(20, 24, 28, 255),
  IM_COL32(34, 39, 46, 255),
  IM_COL32(201, 209, 214, 255),
  IM_COL32(126, 138, 147, 255),
  IM_COL32(58, 67, 76, 255),
  IM_COL32(238, 242, 240, 255),
  IM_COL32(79, 209, 181, 255),
  IM_COL32(242, 184, 75, 255),
  IM_COL32(240, 158, 72, 255),
  IM_COL32(56, 50, 45, 255),
  IM_COL32(229, 72, 77, 255),
  IM_COL32(108, 166, 255, 255),
  IM_COL32(120, 135, 145, 110),
  IM_COL32(120, 130, 138, 255),
  IM_COL32(150, 158, 165, 255),
  IM_COL32(238, 242, 240, 255),
  IM_COL32(0, 0, 0, 90),
  IM_COL32(238, 242, 240, 255),
  IM_COL32(238, 242, 240, 255),
  IM_COL32(233, 236, 230, 255),
  IM_COL32(27, 31, 36, 255),
  IM_COL32(38, 44, 50, 255),
  IM_COL32(128, 134, 136, 255),
  IM_COL32(95, 104, 110, 255),
  IM_COL32(52, 60, 68, 255),
  false,
  {
    IM_COL32(79, 209, 181, 255), IM_COL32(79, 209, 181, 255), IM_COL32(79, 209, 181, 255),
    IM_COL32(79, 209, 181, 255), IM_COL32(79, 209, 181, 255), IM_COL32(79, 209, 181, 255),
    IM_COL32(79, 209, 181, 255), IM_COL32(79, 209, 181, 255),
  },
  {
    { IM_COL32(146, 154, 160, 255), IM_COL32(220, 225, 228, 255), IM_COL32(40, 46, 52, 255) },
    { IM_COL32(17, 20, 24, 255), IM_COL32(40, 46, 53, 255), IM_COL32(230, 236, 234, 255) },
    { IM_COL32(196, 146, 52, 255), IM_COL32(242, 184, 75, 255), IM_COL32(45, 32, 10, 255) },
    { IM_COL32(20, 20, 22, 255), IM_COL32(205, 205, 208, 255), IM_COL32(90, 90, 95, 255) },
  },
  IM_COL32(11, 16, 19, 255),
  IM_COL32(214, 245, 238, 255),
  IM_COL32(111, 143, 138, 255),
  IM_COL32(79, 209, 181, 255),
  IM_COL32(8, 30, 26, 255),
  IM_COL32(40, 55, 60, 255),
  IM_COL32(30, 45, 50, 255),
  IM_COL32(30, 50, 56, 255),
  IM_COL32(22, 36, 40, 255),
  IM_COL32(16, 24, 27, 255),
  IM_COL32(60, 80, 90, 120),
  ImVec4(0.09f, 0.14f, 0.16f, 1.0f),
  ImVec4(0.12f, 0.19f, 0.21f, 1.0f),
  ImVec4(0.15f, 0.24f, 0.26f, 1.0f),
  ImVec4(0.12f, 0.19f, 0.21f, 1.0f),
  ImVec4(0.16f, 0.26f, 0.28f, 1.0f),
  ImVec4(0.50f, 0.88f, 0.78f, 1.0f),
};

const Theme* const kThemes[THEME_LAST] = { &kCream, &kClassic };

int current = THEME_CREAM;

}  // namespace

const Theme& theme() { return *kThemes[current]; }

void SetTheme(int id) {
  if (id >= 0 && id < THEME_LAST) {
    current = id;
  }
}

int theme_index() { return current; }

const Theme& theme_at(int id) { return *kThemes[id >= 0 && id < THEME_LAST ? id : 0]; }

int FindTheme(const std::string& id) {
  for (int i = 0; i < THEME_LAST; ++i) {
    if (id == kThemes[i]->id) {
      return i;
    }
  }
  return THEME_LAST;
}

void ApplyImGuiStyle() {
  ImGuiStyle& style = ImGui::GetStyle();
  ImGui::StyleColorsDark();
  style.FrameRounding = 2.0f;
  style.GrabRounding = 2.0f;
  style.WindowPadding = ImVec2(0, 0);
  style.WindowBorderSize = 0.0f;
  // Screen widgets, in the theme's screen colours (see ui/theme.h).
  const Theme& t = theme();
  const ImVec4 kAccent = ImGui::ColorConvertU32ToFloat4(t.screen_accent);
  ImVec4* c = style.Colors;
  c[ImGuiCol_FrameBg] = t.widget_frame;
  c[ImGuiCol_FrameBgHovered] = t.widget_frame_hovered;
  c[ImGuiCol_FrameBgActive] = t.widget_frame_active;
  c[ImGuiCol_SliderGrab] = kAccent;
  c[ImGuiCol_SliderGrabActive] = t.widget_accent_light;
  c[ImGuiCol_CheckMark] = kAccent;
  c[ImGuiCol_Button] = t.widget_button;
  c[ImGuiCol_ButtonHovered] = t.widget_button_hovered;
  c[ImGuiCol_ButtonActive] = kAccent;
  c[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(t.screen_text);
}

ImU32 TrackColour(int track) {
  const Theme& t = theme();
  return t.track_colours && track >= 0 && track < 8 ? t.tracks[track] : t.accent;
}

}  // namespace drumfake
