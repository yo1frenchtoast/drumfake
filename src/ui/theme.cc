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
  IM_COL32(176, 156, 136, 255),  // pad_dark_dim
  IM_COL32(64, 50, 40, 255),     // pad_ink
  IM_COL32(176, 166, 152, 255),  // pad_muted
  IM_COL32(132, 114, 98, 255),   // pad_label
  IM_COL32(84, 72, 62, 255),     // pad_meter
  true,
  {
    IM_COL32(196, 92, 56, 255),    // terracotta
    IM_COL32(200, 142, 40, 255),   // mustard
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

// Midnight: a navy panel, coral accents, pastel track colours.
const Theme kMidnight = {
  "midnight", "Midnight",
  IM_COL32(14, 18, 32, 255),     // background
  IM_COL32(28, 36, 62, 255),     // panel
  IM_COL32(52, 64, 100, 255),    // panel_edge
  IM_COL32(12, 15, 28, 255),     // bezel
  IM_COL32(36, 46, 78, 255),     // surface
  IM_COL32(16, 20, 36, 255),     // button
  IM_COL32(44, 56, 92, 255),     // button_top
  IM_COL32(222, 228, 240, 255),  // label
  IM_COL32(150, 162, 194, 255),  // label_dim
  IM_COL32(60, 70, 104, 255),    // led_off
  IM_COL32(246, 248, 255, 255),  // led_white
  IM_COL32(255, 122, 98, 255),   // accent: coral
  IM_COL32(255, 196, 92, 255),   // amber
  IM_COL32(255, 150, 90, 255),   // shift
  IM_COL32(70, 62, 84, 255),     // shift_face
  IM_COL32(240, 70, 80, 255),    // red
  IM_COL32(120, 180, 255, 255),  // lock_mod
  IM_COL32(140, 156, 200, 110),  // xy_dots
  IM_COL32(130, 142, 176, 255),  // step_dim
  IM_COL32(170, 180, 206, 255),  // step_last
  IM_COL32(246, 248, 255, 255),  // step_on
  IM_COL32(0, 0, 0, 90),         // page_dim
  IM_COL32(232, 236, 248, 255),  // knob_ring
  IM_COL32(232, 236, 248, 255),  // logo_light
  IM_COL32(232, 236, 246, 255),  // pad_face
  IM_COL32(20, 26, 46, 255),     // pad_dark
  IM_COL32(150, 162, 194, 255),  // pad_dark_dim
  IM_COL32(30, 38, 64, 255),     // pad_ink
  IM_COL32(128, 136, 156, 255),  // pad_muted
  IM_COL32(84, 96, 126, 255),    // pad_label
  IM_COL32(50, 62, 96, 255),     // pad_meter
  true,
  {
    IM_COL32(255, 122, 98, 255),   // coral
    IM_COL32(255, 190, 90, 255),   // amber
    IM_COL32(110, 214, 170, 255),  // mint
    IM_COL32(100, 180, 255, 255),  // sky
    IM_COL32(170, 140, 255, 255),  // lavender
    IM_COL32(255, 120, 180, 255),  // pink
    IM_COL32(190, 220, 90, 255),   // lime
    IM_COL32(255, 170, 130, 255),  // peach
  },
  {
    { IM_COL32(18, 22, 40, 255), IM_COL32(48, 58, 92, 255), IM_COL32(240, 242, 250, 255) },
    { IM_COL32(14, 17, 30, 255), IM_COL32(36, 44, 72, 255), IM_COL32(240, 242, 250, 255) },
    { IM_COL32(214, 92, 74, 255), IM_COL32(255, 122, 98, 255), IM_COL32(40, 16, 12, 255) },
    { IM_COL32(16, 18, 30, 255), IM_COL32(200, 206, 224, 255), IM_COL32(80, 88, 110, 255) },
  },
  IM_COL32(8, 11, 22, 255),      // screen
  IM_COL32(214, 226, 250, 255),  // screen_text
  IM_COL32(116, 130, 166, 255),  // screen_dim
  IM_COL32(255, 140, 116, 255),  // screen_accent
  IM_COL32(40, 12, 8, 255),      // screen_on_accent
  IM_COL32(40, 48, 76, 255),     // screen_rule
  IM_COL32(30, 38, 62, 255),     // screen_frame
  IM_COL32(30, 40, 68, 255),     // screen_tile
  IM_COL32(20, 26, 46, 255),     // screen_well
  IM_COL32(12, 16, 30, 255),     // screen_wave
  IM_COL32(70, 84, 130, 120),    // screen_hover
  ImVec4(0.09f, 0.11f, 0.20f, 1.0f),
  ImVec4(0.12f, 0.15f, 0.27f, 1.0f),
  ImVec4(0.16f, 0.19f, 0.33f, 1.0f),
  ImVec4(0.12f, 0.15f, 0.27f, 1.0f),
  ImVec4(0.17f, 0.21f, 0.36f, 1.0f),
  ImVec4(1.00f, 0.72f, 0.64f, 1.0f),
};

// Sage: a sage-green panel, ivory pads, rust accents, cream knob caps.
const Theme kSage = {
  "sage", "Sage",
  IM_COL32(34, 40, 34, 255),     // background
  IM_COL32(176, 188, 164, 255),  // panel
  IM_COL32(140, 154, 128, 255),  // panel_edge
  IM_COL32(36, 40, 34, 255),     // bezel
  IM_COL32(166, 178, 154, 255),  // surface
  IM_COL32(110, 122, 100, 255),  // button
  IM_COL32(200, 210, 188, 255),  // button_top
  IM_COL32(30, 38, 28, 255),     // label
  IM_COL32(68, 80, 64, 255),     // label_dim
  IM_COL32(130, 142, 120, 255),  // led_off
  IM_COL32(252, 250, 240, 255),  // led_white
  IM_COL32(168, 84, 30, 255),    // accent: rust
  IM_COL32(210, 150, 40, 255),   // amber
  IM_COL32(120, 64, 26, 255),    // shift
  IM_COL32(160, 150, 120, 255),  // shift_face
  IM_COL32(178, 40, 40, 255),    // red
  IM_COL32(52, 92, 140, 255),    // lock_mod
  IM_COL32(96, 112, 90, 120),    // xy_dots
  IM_COL32(92, 104, 86, 255),    // step_dim
  IM_COL32(64, 76, 60, 255),     // step_last
  IM_COL32(252, 250, 240, 255),  // step_on
  IM_COL32(60, 70, 56, 70),      // page_dim
  IM_COL32(30, 38, 28, 255),     // knob_ring
  IM_COL32(30, 38, 28, 255),     // logo_light
  IM_COL32(244, 242, 230, 255),  // pad_face
  IM_COL32(40, 46, 38, 255),     // pad_dark
  IM_COL32(164, 176, 152, 255),  // pad_dark_dim
  IM_COL32(40, 46, 38, 255),     // pad_ink
  IM_COL32(150, 156, 140, 255),  // pad_muted
  IM_COL32(90, 102, 84, 255),    // pad_label
  IM_COL32(70, 80, 64, 255),     // pad_meter
  true,
  {
    IM_COL32(168, 84, 30, 255),    // rust
    IM_COL32(196, 150, 40, 255),   // mustard
    IM_COL32(70, 120, 70, 255),    // forest
    IM_COL32(50, 120, 120, 255),   // teal
    IM_COL32(70, 100, 150, 255),   // denim
    IM_COL32(110, 70, 110, 255),   // aubergine
    IM_COL32(180, 100, 80, 255),   // clay
    IM_COL32(120, 120, 60, 255),   // olive
  },
  {
    { IM_COL32(60, 66, 56, 255), IM_COL32(232, 228, 212, 255), IM_COL32(40, 44, 36, 255) },
    { IM_COL32(30, 34, 28, 255), IM_COL32(58, 64, 54, 255), IM_COL32(240, 236, 220, 255) },
    { IM_COL32(150, 72, 26, 255), IM_COL32(184, 96, 40, 255), IM_COL32(250, 236, 220, 255) },
    { IM_COL32(30, 34, 28, 255), IM_COL32(222, 220, 206, 255), IM_COL32(90, 96, 84, 255) },
  },
  IM_COL32(20, 24, 20, 255),     // screen
  IM_COL32(226, 236, 214, 255),  // screen_text
  IM_COL32(138, 152, 128, 255),  // screen_dim
  IM_COL32(226, 150, 80, 255),   // screen_accent
  IM_COL32(36, 18, 6, 255),      // screen_on_accent
  IM_COL32(52, 62, 50, 255),     // screen_rule
  IM_COL32(42, 50, 40, 255),     // screen_frame
  IM_COL32(44, 52, 42, 255),     // screen_tile
  IM_COL32(30, 36, 30, 255),     // screen_well
  IM_COL32(24, 28, 24, 255),     // screen_wave
  IM_COL32(90, 104, 84, 120),    // screen_hover
  ImVec4(0.14f, 0.17f, 0.14f, 1.0f),
  ImVec4(0.19f, 0.23f, 0.19f, 1.0f),
  ImVec4(0.24f, 0.29f, 0.23f, 1.0f),
  ImVec4(0.19f, 0.23f, 0.19f, 1.0f),
  ImVec4(0.25f, 0.30f, 0.24f, 1.0f),
  ImVec4(0.95f, 0.72f, 0.48f, 1.0f),
};

// Plum: a deep plum panel, gold accents and knob caps.
const Theme kPlum = {
  "plum", "Plum",
  IM_COL32(24, 14, 22, 255),     // background
  IM_COL32(58, 34, 54, 255),     // panel
  IM_COL32(88, 58, 82, 255),     // panel_edge
  IM_COL32(20, 12, 18, 255),     // bezel
  IM_COL32(70, 44, 66, 255),     // surface
  IM_COL32(30, 18, 28, 255),     // button
  IM_COL32(82, 52, 76, 255),     // button_top
  IM_COL32(240, 224, 232, 255),  // label
  IM_COL32(186, 160, 176, 255),  // label_dim
  IM_COL32(96, 70, 90, 255),     // led_off
  IM_COL32(255, 246, 250, 255),  // led_white
  IM_COL32(232, 184, 72, 255),   // accent: gold
  IM_COL32(255, 140, 110, 255),  // amber: salmon (the playhead)
  IM_COL32(246, 164, 64, 255),   // shift
  IM_COL32(96, 74, 70, 255),     // shift_face
  IM_COL32(236, 70, 90, 255),    // red
  IM_COL32(130, 170, 255, 255),  // lock_mod
  IM_COL32(170, 140, 160, 110),  // xy_dots
  IM_COL32(160, 132, 150, 255),  // step_dim
  IM_COL32(200, 176, 190, 255),  // step_last
  IM_COL32(255, 246, 250, 255),  // step_on
  IM_COL32(0, 0, 0, 90),         // page_dim
  IM_COL32(240, 224, 232, 255),  // knob_ring
  IM_COL32(240, 224, 232, 255),  // logo_light
  IM_COL32(246, 236, 240, 255),  // pad_face
  IM_COL32(34, 20, 32, 255),     // pad_dark
  IM_COL32(186, 160, 176, 255),  // pad_dark_dim
  IM_COL32(52, 30, 48, 255),     // pad_ink
  IM_COL32(150, 132, 144, 255),  // pad_muted
  IM_COL32(112, 86, 104, 255),   // pad_label
  IM_COL32(80, 54, 76, 255),     // pad_meter
  true,
  {
    IM_COL32(232, 184, 72, 255),   // gold
    IM_COL32(240, 120, 150, 255),  // rose
    IM_COL32(170, 130, 240, 255),  // violet
    IM_COL32(80, 190, 180, 255),   // teal
    IM_COL32(110, 170, 240, 255),  // sky
    IM_COL32(250, 130, 100, 255),  // coral
    IM_COL32(180, 210, 100, 255),  // lime
    IM_COL32(220, 180, 140, 255),  // sand
  },
  {
    { IM_COL32(150, 116, 50, 255), IM_COL32(226, 196, 120, 255), IM_COL32(50, 30, 20, 255) },
    { IM_COL32(26, 16, 24, 255), IM_COL32(66, 42, 60, 255), IM_COL32(246, 232, 240, 255) },
    { IM_COL32(196, 150, 50, 255), IM_COL32(240, 196, 90, 255), IM_COL32(50, 32, 10, 255) },
    { IM_COL32(22, 14, 20, 255), IM_COL32(220, 204, 214, 255), IM_COL32(96, 80, 90, 255) },
  },
  IM_COL32(14, 8, 14, 255),      // screen
  IM_COL32(246, 228, 238, 255),  // screen_text
  IM_COL32(166, 134, 156, 255),  // screen_dim
  IM_COL32(240, 196, 96, 255),   // screen_accent
  IM_COL32(40, 26, 6, 255),      // screen_on_accent
  IM_COL32(66, 44, 62, 255),     // screen_rule
  IM_COL32(52, 34, 50, 255),     // screen_frame
  IM_COL32(56, 36, 54, 255),     // screen_tile
  IM_COL32(36, 22, 34, 255),     // screen_well
  IM_COL32(26, 16, 24, 255),     // screen_wave
  IM_COL32(110, 80, 104, 120),   // screen_hover
  ImVec4(0.17f, 0.10f, 0.16f, 1.0f),
  ImVec4(0.23f, 0.14f, 0.21f, 1.0f),
  ImVec4(0.29f, 0.18f, 0.27f, 1.0f),
  ImVec4(0.23f, 0.14f, 0.21f, 1.0f),
  ImVec4(0.30f, 0.19f, 0.28f, 1.0f),
  ImVec4(0.98f, 0.86f, 0.58f, 1.0f),
};

// Fake: the original slate panel with its mint accent.
const Theme kFake = {
  "fake", "Fake",
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
  IM_COL32(126, 138, 147, 255),
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

const Theme* const kThemes[THEME_LAST] = { &kMidnight, &kCream, &kSage, &kPlum, &kFake };

int current = THEME_MIDNIGHT;

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
  if (id == "classic") {
    return THEME_FAKE;  // Its name before.
  }
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
