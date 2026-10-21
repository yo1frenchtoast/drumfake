#include "ui/widgets.h"

#include <cmath>

#include "imgui_internal.h"

namespace drumfreake {

namespace {

const float kPi = 3.14159265f;
// Knob travel: from 7:30 to 4:30 (270 degrees).
const float kKnobStart = 0.75f * kPi;
const float kKnobRange = 1.5f * kPi;

ImU32 Shade(ImU32 color, float amount) {
  ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
  c.x = ImClamp(c.x * amount, 0.0f, 1.0f);
  c.y = ImClamp(c.y * amount, 0.0f, 1.0f);
  c.z = ImClamp(c.z * amount, 0.0f, 1.0f);
  return ImGui::ColorConvertFloat4ToU32(c);
}

}  // namespace

void Canvas::Text(float x, float y, float size, ImU32 color, const char* text,
                  bool centered, bool use_bold) const {
  ImFont* f = use_bold && bold ? bold : font;
  float px = S(size);
  ImVec2 p = P(x, y);
  if (centered) {
    ImVec2 extent = f->CalcTextSizeA(px, FLT_MAX, 0.0f, text);
    p.x -= extent.x * 0.5f;
    p.y -= extent.y * 0.5f;
  }
  draw_list->AddText(f, px, p, color, text);
}

Interaction HitBox(const Canvas& c, const char* id, ImVec2 min, ImVec2 max,
                   bool touch_button) {
  Interaction result;
  ImVec2 a = c.P(min);
  ImVec2 b = c.P(max);
  ImGui::SetCursorScreenPos(a);
  ImVec2 size(ImMax(b.x - a.x, 1.0f), ImMax(b.y - a.y, 1.0f));
  result.clicked = ImGui::InvisibleButton(
      id, size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  result.hovered = ImGui::IsItemHovered();
  result.active = ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
  result.pressed = ImGui::IsItemActivated() && ImGui::IsMouseDown(ImGuiMouseButton_Left);
  result.released = ImGui::IsItemDeactivated() &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  result.right_clicked = result.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  if (result.right_clicked) {
    result.clicked = false;
  }
  result.double_clicked = result.hovered &&
      ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  // On a touchscreen, holding still stands in for the right button.
  ImGuiIO& io = ImGui::GetIO();
  result.touch = io.MouseSource == ImGuiMouseSource_TouchScreen;
  const float kLongPress = 0.5f;
  ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
  result.long_pressed = result.active && result.touch &&
      io.MouseDownDuration[0] >= kLongPress && io.MouseDownDurationPrev[0] < kLongPress &&
      drag.x * drag.x + drag.y * drag.y < 15.0f * 15.0f;

  if (touch_button) {
    const ImGuiID item = ImGui::GetItemID();
    if (c.touch_targets) {
      c.touch_targets->push_back({ TOUCH_BUTTON, -1, item, min, max });
    }
    if (c.button_touches) {
      auto it = c.button_touches->find(item);
      if (it != c.button_touches->end()) {
        ButtonTouch& t = it->second;
        result.touch = true;
        result.active = result.active || t.fingers > 0;
        if (t.pressed) {
          result.pressed = true;
          t.pressed = false;
          // A tap shorter than a frame: held for this frame, released on the
          // next one, so that press/release logic sees both.
          result.active = true;
        } else if (t.released) {
          result.released = true;
          result.clicked = true;
          t.released = false;
        }
        if (t.fingers > 0 && !t.long_press_done &&
            ImGui::GetTime() - t.down_time >= kLongPress) {
          result.long_pressed = true;
          t.long_press_done = true;
        }
      }
    }
  }
  return result;
}

bool Knob(const Canvas& c, const char* id, ImVec2 center, float radius,
          float* value, float default_value, KnobStyle style, bool ring,
          ImU32 ring_color, Interaction* interaction,
          const KnobDecor* decor) {
  ImDrawList* dl = c.draw_list;
  const float hit = radius * (ring ? 1.45f : 1.15f);
  ImVec2 hit_min(center.x - hit, center.y - hit);
  ImVec2 hit_max(center.x + hit, center.y + hit);
  Interaction in = HitBox(c, id, hit_min, hit_max, false);
  const ImGuiID item = ImGui::GetItemID();
  if (c.touch_targets) {
    c.touch_targets->push_back({ TOUCH_KNOB, -1, item, hit_min, hit_max });
  }
  KnobTouch* touch = nullptr;
  if (c.knob_touches) {
    auto it = c.knob_touches->find(item);
    if (it != c.knob_touches->end()) {
      touch = &it->second;
    }
  }

  float old_value = *value;
  ImGuiIO& io = ImGui::GetIO();
  if (touch) {
    in.touch_active = touch->active;
    in.double_clicked = in.double_clicked || touch->double_tap;
    touch->double_tap = false;
  }
  if (in.double_clicked) {
    *value = default_value;
  } else if (touch && touch->delta != 0.0f) {
    *value += touch->delta * 0.006f;
  } else if (in.active) {
    float speed = io.KeyShift ? 0.0008f : 0.005f;
    *value -= io.MouseDelta.y * speed;
    *value += io.MouseDelta.x * speed * 0.5f;
  } else if (in.hovered && io.MouseWheel != 0.0f) {
    *value += io.MouseWheel * (io.KeyShift ? 0.002f : 0.01f);
  }
  if (touch) {
    touch->delta = 0.0f;
  }
  *value = ImClamp(*value, 0.0f, 1.0f);
  if (in.active || in.hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
  }
  if (interaction) {
    *interaction = in;
  }

  ImVec2 p = c.P(center);
  float r = c.S(radius);
  float angle = kKnobStart + kKnobRange * *value;

  if (decor && decor->halo) {
    // Soft glow: a few concentric strokes fading outwards.
    float halo_radius = r * (ring ? 1.62f : 1.25f);
    ImVec4 halo = ImGui::ColorConvertU32ToFloat4(decor->halo);
    for (int i = 0; i < 4; ++i) {
      ImVec4 layer = halo;
      layer.w *= 1.0f - i * 0.24f;
      dl->AddCircle(p, halo_radius + c.S(1.6f) * i, ImGui::ColorConvertFloat4ToU32(layer),
                    48, c.S(2.0f));
    }
  }

  if (ring) {
    float ring_radius = r * 1.32f;
    dl->AddCircle(p, ring_radius, colors::kLabelDim, 48, c.S(1.2f));
    dl->PathArcTo(p, ring_radius, kKnobStart, angle, 48);
    dl->PathStroke(ring_color, 0, c.S(2.6f));
    // Position dot at the start of travel, like on the panel.
    ImVec2 dot(p.x + cosf(kKnobStart) * ring_radius * 1.12f,
               p.y + sinf(kKnobStart) * ring_radius * 1.12f);
    dl->AddCircleFilled(dot, c.S(2.0f), colors::kLabel);

    if (decor && decor->marker >= 0.0f && std::fabs(decor->marker - *value) > 0.004f) {
      // Offset between the knob and the value actually playing.
      float marker_angle = kKnobStart + kKnobRange * ImClamp(decor->marker, 0.0f, 1.0f);
      float inner = ring_radius + c.S(3.5f);
      dl->PathArcTo(p, inner, ImMin(angle, marker_angle), ImMax(angle, marker_angle), 32);
      dl->PathStroke(decor->marker_color, 0, c.S(2.0f));
      dl->AddCircleFilled(ImVec2(p.x + cosf(marker_angle) * inner,
                                 p.y + sinf(marker_angle) * inner),
                          c.S(3.2f), decor->marker_color);
    }
    if (decor && decor->automated) {
      ImVec2 badge(p.x + ring_radius * 0.95f, p.y - ring_radius * 0.95f);
      dl->AddCircleFilled(badge, c.S(3.4f),
                          decor->badge_color ? decor->badge_color : colors::kAccent);
      dl->AddCircle(badge, c.S(3.4f), IM_COL32(0, 0, 0, 160), 12, c.S(1.0f));
    }
  }

  // Shadow and body.
  dl->AddCircleFilled(ImVec2(p.x + c.S(2), p.y + c.S(3)), r, IM_COL32(0, 0, 0, 110), 40);
  ImU32 body;
  ImU32 top;
  ImU32 pointer;
  switch (style) {
    case KNOB_BLACK:
      body = IM_COL32(17, 20, 24, 255);
      top = IM_COL32(40, 46, 53, 255);
      pointer = IM_COL32(230, 236, 234, 255);
      break;
    case KNOB_AMBER:
      body = IM_COL32(196, 146, 52, 255);
      top = IM_COL32(242, 184, 75, 255);
      pointer = IM_COL32(45, 32, 10, 255);
      break;
    case KNOB_ENCODER:
      body = IM_COL32(20, 20, 22, 255);
      top = IM_COL32(205, 205, 208, 255);
      pointer = IM_COL32(90, 90, 95, 255);
      break;
    case KNOB_SILVER:
    default:
      body = IM_COL32(146, 154, 160, 255);
      top = IM_COL32(220, 225, 228, 255);
      pointer = IM_COL32(40, 46, 52, 255);
      break;
  }
  dl->AddCircleFilled(p, r, body, 40);
  float cap = style == KNOB_ENCODER ? 0.82f : 0.72f;
  dl->AddCircleFilled(p, r * cap, top, 40);
  dl->AddCircle(p, r * cap, Shade(top, 1.15f), 40, c.S(1.0f));
  if (style == KNOB_ENCODER) {
    // Knurled metal cap: concentric rings instead of a pointer.
    dl->AddCircle(p, r * 0.55f, Shade(top, 0.85f), 40, c.S(1.0f));
    dl->AddCircle(p, r * 0.3f, Shade(top, 0.85f), 40, c.S(1.0f));
  }
  // The encoder turns endlessly: no position to show.
  if (style != KNOB_ENCODER) {
    ImVec2 tip(p.x + cosf(angle) * r * 0.9f, p.y + sinf(angle) * r * 0.9f);
    ImVec2 base(p.x + cosf(angle) * r * 0.35f, p.y + sinf(angle) * r * 0.35f);
    dl->AddLine(base, tip, pointer, c.S(2.4f));
  }

  return *value != old_value;
}

void DrawIcon(const Canvas& c, Icon icon, ImVec2 center, float size, ImU32 color) {
  ImDrawList* dl = c.draw_list;
  ImVec2 p = c.P(center);
  float s = c.S(size) * 0.5f;
  float t = c.S(1.4f);
  switch (icon) {
    case ICON_RECORD:
      dl->AddCircleFilled(p, s * 0.7f, color, 24);
      break;
    case ICON_STOP:
      dl->AddRectFilled(ImVec2(p.x - s * 0.6f, p.y - s * 0.6f),
                        ImVec2(p.x + s * 0.6f, p.y + s * 0.6f), color);
      break;
    case ICON_PLAY:
      dl->AddTriangleFilled(ImVec2(p.x - s * 0.5f, p.y - s * 0.7f),
                            ImVec2(p.x - s * 0.5f, p.y + s * 0.7f),
                            ImVec2(p.x + s * 0.7f, p.y), color);
      break;
    case ICON_SONG:
      dl->AddRect(ImVec2(p.x - s * 0.6f, p.y - s * 0.8f),
                  ImVec2(p.x + s * 0.6f, p.y + s * 0.8f), color, 0, 0, t);
      for (int i = -1; i <= 1; ++i) {
        dl->AddLine(ImVec2(p.x - s * 0.35f, p.y + i * s * 0.35f),
                    ImVec2(p.x + s * 0.35f, p.y + i * s * 0.35f), color, t);
      }
      break;
    case ICON_SAMPLE: {
      const float heights[] = { 0.2f, 0.5f, 0.9f, 0.4f, 0.7f, 1.0f, 0.5f, 0.8f, 0.3f, 0.15f };
      for (int i = 0; i < 10; ++i) {
        float x = p.x - s * 1.2f + i * s * 0.26f;
        dl->AddLine(ImVec2(x, p.y - heights[i] * s * 0.7f),
                    ImVec2(x, p.y + heights[i] * s * 0.7f), color, t);
      }
      break;
    }
    case ICON_BROWSE:
      dl->AddCircle(ImVec2(p.x - s * 0.15f, p.y - s * 0.15f), s * 0.5f, color, 20, t);
      dl->AddLine(ImVec2(p.x + s * 0.2f, p.y + s * 0.2f),
                  ImVec2(p.x + s * 0.7f, p.y + s * 0.7f), color, t * 1.5f);
      break;
    case ICON_EDIT:
      dl->AddCircle(p, s * 0.35f, color, 16, t);
      for (int i = 0; i < 8; ++i) {
        float a = i * kPi / 4.0f;
        dl->AddLine(ImVec2(p.x + cosf(a) * s * 0.5f, p.y + sinf(a) * s * 0.5f),
                    ImVec2(p.x + cosf(a) * s * 0.75f, p.y + sinf(a) * s * 0.75f),
                    color, t * 1.4f);
      }
      break;
    case ICON_SAVE:
      dl->AddRect(ImVec2(p.x - s * 0.7f, p.y - s * 0.7f),
                  ImVec2(p.x + s * 0.7f, p.y + s * 0.7f), color, c.S(1.5f), 0, t);
      dl->AddRect(ImVec2(p.x - s * 0.35f, p.y - s * 0.7f),
                  ImVec2(p.x + s * 0.35f, p.y - s * 0.2f), color, 0, 0, t);
      dl->AddRect(ImVec2(p.x - s * 0.45f, p.y + s * 0.15f),
                  ImVec2(p.x + s * 0.45f, p.y + s * 0.7f), color, 0, 0, t);
      break;
    case ICON_BACK:
      dl->PathArcTo(p, s * 0.55f, -kPi * 0.9f, kPi * 0.6f, 20);
      dl->PathStroke(color, 0, t);
      dl->AddTriangleFilled(ImVec2(p.x - s * 0.75f, p.y - s * 0.35f),
                            ImVec2(p.x - s * 0.25f, p.y - s * 0.35f),
                            ImVec2(p.x - s * 0.5f, p.y + s * 0.05f), color);
      break;
    case ICON_SOUND:
      dl->AddCircle(ImVec2(p.x, p.y + s * 0.15f), s * 0.55f, color, 20, t);
      dl->AddLine(ImVec2(p.x, p.y - s * 0.4f), ImVec2(p.x, p.y - s * 0.8f), color, t);
      break;
    case ICON_FILTER:
      dl->AddTriangle(ImVec2(p.x - s * 0.7f, p.y - s * 0.6f),
                      ImVec2(p.x + s * 0.7f, p.y - s * 0.6f),
                      ImVec2(p.x, p.y + s * 0.2f), color, t);
      dl->AddLine(ImVec2(p.x, p.y + s * 0.2f), ImVec2(p.x, p.y + s * 0.8f), color, t);
      break;
    case ICON_TRACK:
      dl->AddLine(ImVec2(p.x - s * 0.7f, p.y + s * 0.6f), ImVec2(p.x - s * 0.1f, p.y - s * 0.6f), color, t);
      dl->AddLine(ImVec2(p.x - s * 0.1f, p.y - s * 0.6f), ImVec2(p.x + s * 0.3f, p.y + s * 0.2f), color, t);
      dl->AddLine(ImVec2(p.x + s * 0.3f, p.y + s * 0.2f), ImVec2(p.x + s * 0.7f, p.y - s * 0.3f), color, t);
      break;
    case ICON_MASTER:
      dl->AddLine(ImVec2(p.x - s * 0.6f, p.y - s * 0.6f), ImVec2(p.x, p.y), color, t);
      dl->AddLine(ImVec2(p.x + s * 0.6f, p.y - s * 0.6f), ImVec2(p.x, p.y), color, t);
      dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x, p.y + s * 0.7f), color, t);
      break;
    case ICON_MODS:
      for (int i = 0; i < 3; ++i) {
        float x = p.x + (i - 1) * s * 0.45f;
        dl->AddLine(ImVec2(x, p.y - s * 0.7f), ImVec2(x, p.y + s * 0.7f), color, t);
        float y = p.y + ((i % 2) ? -0.3f : 0.3f) * s;
        dl->AddLine(ImVec2(x - s * 0.15f, y), ImVec2(x + s * 0.15f, y), color, t * 1.6f);
      }
      break;
    case ICON_ENVS:
      // Fast attack, then a decay with one bounce.
      dl->AddLine(ImVec2(p.x - s * 0.8f, p.y + s * 0.6f), ImVec2(p.x - s * 0.4f, p.y - s * 0.7f), color, t);
      dl->AddLine(ImVec2(p.x - s * 0.4f, p.y - s * 0.7f), ImVec2(p.x + s * 0.1f, p.y + s * 0.6f), color, t);
      dl->AddLine(ImVec2(p.x + s * 0.1f, p.y + s * 0.6f), ImVec2(p.x + s * 0.35f, p.y + s * 0.05f), color, t);
      dl->AddLine(ImVec2(p.x + s * 0.35f, p.y + s * 0.05f), ImVec2(p.x + s * 0.6f, p.y + s * 0.6f), color, t);
      break;
    case ICON_CLOUDS:
      dl->AddCircle(ImVec2(p.x - s * 0.35f, p.y + s * 0.1f), s * 0.4f, color, 16, t);
      dl->AddCircle(ImVec2(p.x + s * 0.25f, p.y - s * 0.15f), s * 0.5f, color, 16, t);
      dl->AddCircle(ImVec2(p.x + s * 0.4f, p.y + s * 0.3f), s * 0.3f, color, 16, t);
      break;
    case ICON_ASSIGN:
      dl->AddCircle(p, s * 0.6f, color, 20, t);
      dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + s * 0.4f, p.y - s * 0.4f), color, t);
      break;
    case ICON_MUTE:
      dl->AddRectFilled(ImVec2(p.x - s * 0.8f, p.y - s * 0.25f),
                        ImVec2(p.x - s * 0.45f, p.y + s * 0.25f), color);
      dl->AddTriangleFilled(ImVec2(p.x - s * 0.45f, p.y - s * 0.25f),
                            ImVec2(p.x - s * 0.45f, p.y + s * 0.25f),
                            ImVec2(p.x - s * 0.05f, p.y + s * 0.6f), color);
      dl->AddTriangleFilled(ImVec2(p.x - s * 0.45f, p.y - s * 0.25f),
                            ImVec2(p.x - s * 0.05f, p.y - s * 0.6f),
                            ImVec2(p.x - s * 0.05f, p.y + s * 0.6f), color);
      dl->AddLine(ImVec2(p.x + s * 0.2f, p.y - s * 0.3f), ImVec2(p.x + s * 0.75f, p.y + s * 0.3f), color, t);
      dl->AddLine(ImVec2(p.x + s * 0.75f, p.y - s * 0.3f), ImVec2(p.x + s * 0.2f, p.y + s * 0.3f), color, t);
      break;
    case ICON_KEYBOARD:
      dl->AddRect(ImVec2(p.x - s * 0.9f, p.y - s * 0.6f),
                  ImVec2(p.x + s * 0.9f, p.y + s * 0.6f), color, 0, 0, t);
      for (int i = 1; i < 5; ++i) {
        float x = p.x - s * 0.9f + i * s * 0.36f;
        dl->AddLine(ImVec2(x, p.y - s * 0.6f), ImVec2(x, p.y + s * 0.6f), color, t * 0.8f);
        dl->AddRectFilled(ImVec2(x - s * 0.08f, p.y - s * 0.6f),
                          ImVec2(x + s * 0.08f, p.y), color);
      }
      break;
    case ICON_SPICE:
      dl->PathArcTo(ImVec2(p.x - s * 0.1f, p.y - s * 0.6f), s * 1.1f, kPi * 0.35f, kPi * 0.62f, 12);
      dl->PathStroke(color, 0, t * 2.5f);
      dl->AddLine(ImVec2(p.x + s * 0.1f, p.y - s * 0.6f), ImVec2(p.x + s * 0.35f, p.y - s * 0.85f), color, t);
      break;
    case ICON_FX:
      dl->AddLine(ImVec2(p.x - s * 0.7f, p.y + s * 0.5f), ImVec2(p.x + s * 0.7f, p.y - s * 0.5f), color, t);
      dl->AddCircleFilled(ImVec2(p.x + s * 0.7f, p.y - s * 0.5f), s * 0.2f, color);
      dl->AddLine(ImVec2(p.x - s * 0.2f, p.y - s * 0.6f), ImVec2(p.x - s * 0.2f, p.y - s * 0.2f), color, t);
      dl->AddLine(ImVec2(p.x - s * 0.4f, p.y - s * 0.4f), ImVec2(p.x, p.y - s * 0.4f), color, t);
      break;
    case ICON_LOOPER:
      dl->AddCircle(ImVec2(p.x - s * 0.4f, p.y), s * 0.4f, color, 16, t);
      dl->AddCircle(ImVec2(p.x + s * 0.4f, p.y), s * 0.4f, color, 16, t);
      break;
    case ICON_XY:
      c.Text(center.x, center.y, size * 0.9f, color, "X|Y", true, true);
      break;
    case ICON_NONE:
      break;
  }
}

Interaction Button(const Canvas& c, const char* id, ImVec2 min, ImVec2 max,
                   const char* face_text, const char* label, Icon icon,
                   bool lit, ImU32 lit_color, bool led, ImU32 face_color) {
  Interaction in = HitBox(c, id, min, max);
  ImDrawList* dl = c.draw_list;
  ImVec2 a = c.P(min);
  ImVec2 b = c.P(max);
  float rounding = c.S(4.0f);
  float press = in.active ? c.S(1.5f) : 0.0f;

  dl->AddRectFilled(ImVec2(a.x + c.S(1), a.y + c.S(3)), ImVec2(b.x + c.S(1), b.y + c.S(3)),
                    IM_COL32(0, 0, 0, 120), rounding);
  ImVec2 fa(a.x, a.y + press);
  ImVec2 fb(b.x, b.y + press);
  const ImU32 rubber = face_color ? face_color : colors::kButtonTop;
  ImU32 face = in.hovered ? Shade(rubber, 1.15f) : rubber;
  if (lit && !led) {
    // Backlit: tint the rubber with the light color.
    ImVec4 tint = ImGui::ColorConvertU32ToFloat4(lit_color);
    face = ImGui::ColorConvertFloat4ToU32(ImLerp(
        ImGui::ColorConvertU32ToFloat4(face), ImVec4(tint.x, tint.y, tint.z, 1.0f),
        0.42f * tint.w));
  }
  dl->AddRectFilled(fa, fb, colors::kButton, rounding);
  dl->AddRectFilled(ImVec2(fa.x + c.S(2), fa.y + c.S(1.5f)),
                    ImVec2(fb.x - c.S(2), fb.y - c.S(3)), face, rounding);

  ImU32 content = lit && !led ? lit_color : colors::kLabel;
  ImVec2 mid((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f + press / c.scale - 1.0f);
  if (lit && !led) {
    // Backlit button: soft glow around the face.
    dl->AddRect(ImVec2(fa.x - c.S(1), fa.y - c.S(1)), ImVec2(fb.x + c.S(1), fb.y + c.S(1)),
                (lit_color & 0x00ffffff) | 0x70000000, rounding, 0, c.S(2.0f));
  }
  const float width = max.x - min.x;
  const float height = max.y - min.y;
  const bool has_text = face_text && *face_text;
  // Text shrinks to fit the button's width.
  auto fitted_size = [&](float size) {
    if (!has_text) {
      return size;
    }
    float text_width = c.font->CalcTextSizeA(c.S(size), FLT_MAX, 0, face_text).x / c.scale;
    return text_width > width - 10.0f ? size * (width - 10.0f) / text_width : size;
  };
  if (icon != ICON_NONE && has_text) {
    // Icon above, text below.
    float icon_size = ImMin(width, height) * 0.36f;
    DrawIcon(c, icon, ImVec2(mid.x, mid.y - height * 0.14f), icon_size, content);
    c.Text(mid.x, mid.y + height * 0.24f, fitted_size(ImMin(9.5f, height * 0.2f)), content,
           face_text);
  } else if (icon != ICON_NONE) {
    DrawIcon(c, icon, mid, ImMin(width, height) * 0.5f, content);
  } else if (has_text) {
    c.Text(mid.x, mid.y, fitted_size(ImMin(11.0f, height * 0.3f)), content, face_text);
  }
  if (led) {
    ImVec2 led_pos(min.x + 7.0f, min.y + 7.0f);
    c.draw_list->AddCircleFilled(c.P(led_pos), c.S(2.6f), lit ? lit_color : colors::kLedOff);
    if (lit) {
      c.draw_list->AddCircleFilled(c.P(led_pos), c.S(5.0f), (lit_color & 0x00ffffff) | 0x40000000);
    }
  }
  if (label && *label) {
    c.Text((min.x + max.x) * 0.5f, max.y + 11.0f, 11.0f, colors::kLabel, label, true, true);
  }
  return in;
}

}  // namespace drumfreake
