#include "ui/faceplate.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "core/presets.h"
#include "core/project_io.h"
#include "imgui_internal.h"

namespace drumfreake {

namespace {

// Pads 1-8 in keyboard mode: a major scale.
const int kScaleNotes[8] = { 0, 2, 4, 5, 7, 9, 11, 12 };

// Panel grid. The right-hand side is 16 evenly spaced step columns; each pad
// spans two columns, the screen the first eight and the Sound Edit block the
// last eight. Rows are shared with the left "Perform" column.
namespace layout {
const float kPanelX0 = 20.0f;
const float kPanelY0 = 15.0f;
const float kPanelX1 = 1520.0f;
const float kPanelY1 = 800.0f;
const float kGap = 12.0f;

const float kLeftX0 = 50.0f;
const float kLeftX1 = 350.0f;

const float kRightX0 = 396.0f;
const float kRightX1 = 1490.0f;
const float kStepPitch = (kRightX1 - kRightX0 + kGap) / 16.0f;
const float kStepWidth = kStepPitch - kGap;
const float kPadWidth = 2.0f * kStepPitch - kGap;
const float kScreenX1 = kRightX0 + 8.0f * kStepPitch - kGap;
const float kSoundX0 = kRightX0 + 8.0f * kStepPitch;

const float kTopY0 = 45.0f;
const float kButtonRowY0 = 236.0f;
const float kButtonRowY1 = 284.0f;
const float kSectionY = 312.0f;
const float kFunctionRowY0 = 326.0f;
const float kFunctionRowY1 = 366.0f;
const float kStepRowY0 = 378.0f;
const float kStepRowY1 = 428.0f;
const float kPadRow1Y0 = 450.0f;
const float kPadRow1Y1 = 602.0f;
const float kPadRow2Y0 = 618.0f;
const float kPadRow2Y1 = 770.0f;

inline float StepX(int column) {
  return kRightX0 + column * kStepPitch;
}

inline float PadX(int pad) {
  return kRightX0 + pad * 2.0f * kStepPitch;
}
}  // namespace layout

const ImGuiKey kTrackPadKeys[kNumTracks] = {
  ImGuiKey_Q, ImGuiKey_W, ImGuiKey_E, ImGuiKey_R,
  ImGuiKey_T, ImGuiKey_Y, ImGuiKey_U, ImGuiKey_I
};
const ImGuiKey kLowerPadKeys[8] = {
  ImGuiKey_A, ImGuiKey_S, ImGuiKey_D, ImGuiKey_F,
  ImGuiKey_G, ImGuiKey_H, ImGuiKey_J, ImGuiKey_K
};

ImU32 WithAlpha(ImU32 color, int alpha) {
  return (color & 0x00ffffff) | (static_cast<ImU32>(alpha) << 24);
}

std::string ChordName(uint8_t mask) {
  std::string name;
  for (int t = 0; t < kNumTracks; ++t) {
    if (mask & (1 << t)) {
      if (!name.empty()) {
        name += "+";
      }
      name += static_cast<char>('A' + t);
    }
  }
  return name.empty() ? "-" : name;
}

}  // namespace

void Faceplate::Init(Engine* engine, const std::string& project_path,
                     ImFont* font, ImFont* bold) {
  engine_ = engine;
  project_path_ = project_path;
  canvas_.font = font;
  canvas_.bold = bold;
  clip_step_.Clear();
  clip_track_.Clear();
}

Pattern& Faceplate::pattern() {
  return engine_->project().patterns[engine_->current_pattern()];
}

void Faceplate::Toast(const std::string& message) {
  toast_ = message;
  toast_time_ = ImGui::GetTime();
}

// --- Actions -------------------------------------------------------------------

void Faceplate::TogglePlay() {
  if (engine_->playing()) {
    engine_->Stop();
  } else {
    engine_->Play();
  }
}

void Faceplate::TrackPadDown(int track) {
  if (chord_edit_ >= 0) {
    engine_->project().chords[chord_edit_] ^= 1 << track;
    return;
  }
  if (mute_mode_) {
    engine_->muted[track] = !engine_->muted[track];
    return;
  }
  engine_->selected_track = track;
  if (select_mode_) {
    return;
  }
  track_pad_held_[track] = true;
  engine_->LiveTrigger(track, ImGui::GetIO().KeyShift ? 1.0f : 0.8f, 0);
  UpdateRepeat();
}

void Faceplate::TrackPadUp(int track) {
  track_pad_held_[track] = false;
  UpdateRepeat();
}

void Faceplate::LowerPadDown(int pad) {
  Project& project = engine_->project();
  int track = engine_->selected_track;
  switch (pad_mode_) {
    case PADS_SOUNDS: {
      int preset = preset_bank_ * 8 + pad;
      ApplyPreset(preset, &project.sounds[track]);
      engine_->LiveTrigger(track, 0.8f, 0);
      Toast(std::string("Track ") + static_cast<char>('A' + track) + ": " + preset_name(preset));
      break;
    }
    case PADS_REPEATS:
      lower_pad_held_[pad] = true;
      UpdateRepeat();
      break;
    case PADS_CHORDS:
      for (int t = 0; t < kNumTracks; ++t) {
        if (project.chords[pad] & (1 << t)) {
          engine_->LiveTrigger(t, 0.8f, 0);
        }
      }
      break;
    case PADS_KEYS:
      if (held_step_ >= 0) {
        PushUndo();
        Step& step = pattern().tracks[track].steps[held_step_];
        step.on = true;
        step.note = kScaleNotes[pad];
      }
      engine_->LiveTrigger(track, 0.8f, kScaleNotes[pad]);
      break;
  }
}

void Faceplate::LowerPadUp(int pad) {
  lower_pad_held_[pad] = false;
  UpdateRepeat();
}

void Faceplate::UpdateRepeat() {
  int rate = -1;
  for (int i = 0; i < 8; ++i) {
    if (lower_pad_held_[i]) {
      rate = i;
    }
  }
  if (pad_mode_ != PADS_REPEATS || rate < 0) {
    engine_->SetRepeat(0, 0);
    return;
  }
  uint8_t mask = 0;
  for (int t = 0; t < kNumTracks; ++t) {
    if (track_pad_held_[t]) {
      mask |= 1 << t;
    }
  }
  if (!mask) {
    mask = 1 << engine_->selected_track;
  }
  engine_->SetRepeat(mask, rate);
}

void Faceplate::Tap() {
  double now = ImGui::GetTime();
  if (!taps_.empty() && now - taps_.back() > 2.0) {
    taps_.clear();
  }
  taps_.push_back(now);
  if (taps_.size() > 5) {
    taps_.erase(taps_.begin());
  }
  if (taps_.size() >= 2) {
    double interval = (taps_.back() - taps_.front()) / (taps_.size() - 1);
    engine_->project().bpm = std::clamp(static_cast<float>(60.0 / interval), 30.0f, 300.0f);
  }
}

void Faceplate::PushUndo() {
  undo_.push_back({ engine_->current_pattern(), std::make_unique<Pattern>(pattern()) });
  if (undo_.size() > 32) {
    undo_.pop_front();
  }
}

void Faceplate::Undo() {
  if (song_edit_) {
    Project& project = engine_->project();
    if (project.song_length > 0) {
      --project.song_length;
      song_edit_fresh_ = false;
      Toast("Last pattern removed from the song");
    }
    return;
  }
  if (undo_.empty()) {
    Toast("Nothing to undo");
    return;
  }
  UndoEntry& entry = undo_.back();
  engine_->project().patterns[entry.pattern] = *entry.data;
  undo_.pop_back();
  Toast("Undo");
}

void Faceplate::Copy() {
  int track = engine_->selected_track;
  if (held_step_ >= 0) {
    clip_step_ = pattern().tracks[track].steps[held_step_];
    clipboard_kind_ = CLIP_STEP;
    Toast("Step copied");
  } else if (patterns_mode_) {
    clip_pattern_ = std::make_unique<Pattern>(pattern());
    clipboard_kind_ = CLIP_PATTERN;
    Toast("Pattern copied");
  } else {
    clip_track_ = pattern().tracks[track];
    clipboard_kind_ = CLIP_TRACK;
    Toast(std::string("Track ") + static_cast<char>('A' + track) + " copied");
  }
}

void Faceplate::Paste() {
  int track = engine_->selected_track;
  if (clipboard_kind_ == CLIP_STEP && held_step_ >= 0) {
    PushUndo();
    pattern().tracks[track].steps[held_step_] = clip_step_;
    Toast("Step pasted");
  } else if (clipboard_kind_ == CLIP_PATTERN && clip_pattern_) {
    PushUndo();
    pattern() = *clip_pattern_;
    Toast("Pattern pasted");
  } else if (clipboard_kind_ == CLIP_TRACK) {
    PushUndo();
    pattern().tracks[track] = clip_track_;
    Toast("Track pasted");
  } else {
    Toast("Clipboard empty");
  }
}

void Faceplate::Erase() {
  if (song_edit_) {
    engine_->project().song_length = 0;
    engine_->SetSongMode(false);
    song_edit_fresh_ = false;
    Toast("Song cleared");
    return;
  }
  PushUndo();
  int track = engine_->selected_track;
  if (held_step_ >= 0) {
    pattern().tracks[track].steps[held_step_].Clear();
    Toast("Step erased");
  } else if (patterns_mode_) {
    pattern().Clear();
    Toast("Pattern erased");
  } else {
    int length = pattern().tracks[track].length;
    pattern().tracks[track].Clear();
    pattern().tracks[track].length = length;
    Toast(std::string("Track ") + static_cast<char>('A' + track) + " erased");
  }
}

void Faceplate::Save() {
  std::string error;
  if (SaveProject(engine_->project(), project_path_, &error)) {
    Toast("Saved");
  } else {
    Toast("Save failed: " + error);
  }
}

float* Faceplate::KnobTarget(int knob, bool* is_lock) {
  *is_lock = false;
  Project& project = engine_->project();
  switch (page_) {
    case PAGE_MASTER:
      return &project.master[knob];
    case PAGE_ASSIGN:
      return &project.assign[knob];
    default:
      break;
  }
  int param = (page_ == PAGE_MODS ? P_LFO1_RATE : page_ * kKnobsPerPage) + knob;
  return &project.sounds[engine_->selected_track].params[param];
}

// --- Drawing -------------------------------------------------------------------

void Faceplate::Draw() {
  ImGuiIO& io = ImGui::GetIO();
  ImVec2 size = io.DisplaySize;
  float scale = std::min(size.x / kPanelWidth, size.y / kPanelHeight);
  canvas_.scale = scale;
  canvas_.origin = ImVec2((size.x - kPanelWidth * scale) * 0.5f,
                          (size.y - kPanelHeight * scale) * 0.5f);
  canvas_.draw_list = ImGui::GetWindowDrawList();
  // Fonts are rasterized at 28 px; ImGui widgets on the screen use ~11 px.
  ImGui::SetWindowFontScale(11.0f * scale / 28.0f);

  std::lock_guard<std::mutex> lock(engine_->mutex());

  double now = ImGui::GetTime();
  for (int t = 0; t < kNumTracks; ++t) {
    uint32_t count = engine_->trigger_count[t].load(std::memory_order_relaxed);
    if (count != seen_triggers_[t]) {
      seen_triggers_[t] = count;
      flash_time_[t] = now;
    }
  }

  DrawChrome();
  DrawTopLeft();
  DrawXySection();
  DrawScreenSection();
  DrawSoundEdit();
  DrawSequencer();
  DrawPads();
  HandleKeyboard();
}

void Faceplate::DrawChrome() {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, colors::kBackground);
  dl->AddRectFilled(c.P(layout::kPanelX0 + 5, layout::kPanelY0 + 7),
                    c.P(layout::kPanelX1 + 5, layout::kPanelY1 + 7),
                    IM_COL32(0, 0, 0, 150), c.S(20));
  dl->AddRectFilled(c.P(layout::kPanelX0, layout::kPanelY0),
                    c.P(layout::kPanelX1, layout::kPanelY1), colors::kPanel, c.S(20));
  dl->AddRect(c.P(layout::kPanelX0, layout::kPanelY0),
              c.P(layout::kPanelX1, layout::kPanelY1), colors::kPanelEdge, c.S(20), 0,
              c.S(1.5f));

  // Logo: "DRUM" + bold "F(RE)AKE", like the Freak family logos, with the
  // "(RE)" lit.
  const float kLogoSize = 26.0f;
  float x = layout::kLeftX0;
  auto logo_part = [&](const char* text, ImU32 color, bool use_bold) {
    c.Text(x, layout::kTopY0 - 4, kLogoSize, color, text, false, use_bold);
    ImFont* f = use_bold ? c.bold : c.font;
    x += f->CalcTextSizeA(c.S(kLogoSize), FLT_MAX, 0, text).x / c.scale;
  };
  logo_part("DRUM", colors::kLedWhite, false);
  logo_part("F", colors::kLedWhite, true);
  logo_part("(RE)", colors::kAccent, true);
  logo_part("AKE", colors::kLedWhite, true);
  c.Text(layout::kLeftX0 + 1, layout::kTopY0 + 31, 8, colors::kLabel,
         "OPEN SOURCE ALGORITHMIC DRUM MACHINE", false, true);
  c.Text(layout::kLeftX0 + 1, layout::kTopY0 + 43, 7.5f, colors::kLabelDim,
         "Plaits, Grids & Rings DSP by Mutable Instruments", false);

  // Section titles with a rule, all on the same baselines.
  auto section = [&](float x0, float x1, float y, const char* title) {
    c.Text(x0, y - 6, 9.5f, colors::kLabel, title, false, true);
    float w = c.bold->CalcTextSizeA(c.S(9.5f), FLT_MAX, 0, title).x / c.scale;
    dl->AddLine(c.P(x0 + w + 10, y), c.P(x1, y), colors::kPanelEdge, c.S(1.2f));
  };
  section(layout::kSoundX0, layout::kRightX1, layout::kTopY0 + 2, "SOUND EDIT");
  section(layout::kLeftX0, layout::kLeftX1, layout::kSectionY, "PERFORM");
  section(layout::kRightX0, layout::kRightX1, layout::kSectionY, "SEQUENCER");
}

void Faceplate::DrawTopLeft() {
  const Canvas& c = canvas_;
  Project& project = engine_->project();
  const float knob_y = 150.0f;
  const float label_y = knob_y + 42.0f;

  Knob(c, "##volume", ImVec2(layout::kLeftX0 + 45, knob_y), 27, &project.volume, 0.8f,
       KNOB_BLACK, false, 0);
  c.Text(layout::kLeftX0 + 45, label_y, 9.5f, colors::kLabel, "Volume", true, true);

  float tempo = (project.bpm - 30.0f) / 270.0f;
  if (Knob(c, "##tempo", ImVec2(layout::kLeftX0 + 150, knob_y), 27, &tempo, 90.0f / 270.0f,
           KNOB_AMBER, false, 0)) {
    project.bpm = std::round((30.0f + tempo * 270.0f) * 10.0f) / 10.0f;
  }
  c.Text(layout::kLeftX0 + 150, label_y, 9.5f, colors::kLabel, "Tempo", true, true);

  // TAP, with the metronome LED under it.
  ImVec2 tap_min(layout::kLeftX1 - 76, knob_y - 26);
  ImVec2 tap_max(layout::kLeftX1, knob_y + 20);
  if (Button(c, "##tap", tap_min, tap_max, "TAP", "", ICON_NONE, false, 0).pressed) {
    Tap();
  }
  bool beat = engine_->pattern_step.load() >= 0 && (engine_->pattern_step.load() % 4) == 0;
  ImU32 led = engine_->metronome ? (beat ? colors::kAmber : colors::kAccent) : colors::kLedOff;
  float metro_x = (tap_min.x + tap_max.x) * 0.5f;
  c.draw_list->AddCircleFilled(c.P(tap_min.x + 8, label_y), c.S(3.0f), led);
  c.Text(metro_x + 5, label_y, 8.5f, colors::kLabel, "Metronome", true, true);
  if (HitBox(c, "##metronome", ImVec2(tap_min.x, label_y - 9), ImVec2(tap_max.x, label_y + 9))
          .clicked) {
    engine_->metronome = !engine_->metronome;
    Toast(engine_->metronome ? "Metronome on" : "Metronome off");
  }

  // Transport, on the same row as the buttons under the screen.
  float w = (layout::kLeftX1 - layout::kLeftX0 - 2 * layout::kGap) / 3.0f;
  auto column = [&](int i) { return layout::kLeftX0 + i * (w + layout::kGap); };
  if (Button(c, "##rec", ImVec2(column(0), layout::kButtonRowY0),
             ImVec2(column(0) + w, layout::kButtonRowY1), "", "", ICON_RECORD,
             engine_->recording, colors::kRed).pressed) {
    engine_->recording = !engine_->recording;
  }
  if (Button(c, "##stop", ImVec2(column(1), layout::kButtonRowY0),
             ImVec2(column(1) + w, layout::kButtonRowY1), "", "", ICON_STOP,
             false, 0).pressed) {
    engine_->Stop();
  }
  if (Button(c, "##play", ImVec2(column(2), layout::kButtonRowY0),
             ImVec2(column(2) + w, layout::kButtonRowY1), "", "", ICON_PLAY,
             engine_->playing(), colors::kAccent).pressed) {
    TogglePlay();
  }

  // Pad modes: a 2x2 block level with the lower row of pads.
  struct ModeButton {
    const char* id;
    const char* label;
    Icon icon;
    PadMode mode;
  };
  const ModeButton kPadModes[4] = {
    { "##chords", "Chords", ICON_NONE, PADS_CHORDS },
    { "##sounds", "Sounds", ICON_NONE, PADS_SOUNDS },
    { "##repeats", "Repeats", ICON_NONE, PADS_REPEATS },
    { "##keys", "Keys", ICON_KEYBOARD, PADS_KEYS },
  };
  float half = (layout::kLeftX1 - layout::kLeftX0 - layout::kGap) * 0.5f;
  float row_h = (layout::kPadRow2Y1 - layout::kPadRow2Y0 - layout::kGap) * 0.5f;
  for (int i = 0; i < 4; ++i) {
    const ModeButton& m = kPadModes[i];
    float x0 = layout::kLeftX0 + (i % 2) * (half + layout::kGap);
    float y0 = layout::kPadRow2Y0 + (i / 2) * (row_h + layout::kGap);
    if (Button(c, m.id, ImVec2(x0, y0), ImVec2(x0 + half, y0 + row_h), m.label, "", m.icon,
               pad_mode_ == m.mode, colors::kAccent).pressed) {
      if (m.mode == PADS_SOUNDS && pad_mode_ == PADS_SOUNDS) {
        preset_bank_ = 1 - preset_bank_;
        Toast(preset_bank_ ? "Sounds: bank 2" : "Sounds: bank 1");
      }
      pad_mode_ = m.mode;
    }
  }
  UpdateRepeat();
}

void Faceplate::DrawXySection() {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;

  struct ModeButton {
    const char* id;
    Icon icon;
    const char* label;
    XyMode mode;
  };
  const ModeButton kModes[4] = {
    { "##spice", ICON_SPICE, "Spice", XY_GRIDS },
    { "##fx", ICON_FX, "FX", XY_FX },
    { "##looper", ICON_LOOPER, "Looper", XY_LOOPER },
    { "##macros", ICON_XY, "Macros", XY_MACROS },
  };
  float w = (layout::kLeftX1 - layout::kLeftX0 - 3 * layout::kGap) / 4.0f;
  for (int i = 0; i < 4; ++i) {
    const ModeButton& m = kModes[i];
    float x0 = layout::kLeftX0 + i * (w + layout::kGap);
    bool lit = engine_->xy_mode == m.mode;
    ImU32 led = colors::kLedWhite;
    if (m.mode == XY_GRIDS && engine_->grids_enabled) {
      lit = true;
      led = colors::kAccent;
    }
    Interaction in = Button(c, m.id, ImVec2(x0, layout::kFunctionRowY0),
                            ImVec2(x0 + w, layout::kFunctionRowY1), m.label, "", m.icon,
                            lit, led, true);
    if (in.pressed) {
      if (m.mode == XY_GRIDS && engine_->xy_mode == XY_GRIDS) {
        engine_->grids_enabled = !engine_->grids_enabled;
        Toast(engine_->grids_enabled ? "Grids on (tracks A-C)" : "Grids off");
      }
      engine_->xy_mode = m.mode;
    }
  }

  // Touch surface, from the top of the steps to the bottom of the upper pads.
  ImVec2 min(layout::kLeftX0, layout::kStepRowY0);
  ImVec2 max(layout::kLeftX1, layout::kPadRow1Y1);
  dl->AddRectFilled(c.P(min.x, min.y + 3), c.P(max.x, max.y + 3), IM_COL32(0, 0, 0, 110), c.S(8));
  dl->AddRectFilled(c.P(min), c.P(max), colors::kSurface, c.S(8));
  dl->AddRect(c.P(min), c.P(max), colors::kPanelEdge, c.S(8), 0, c.S(1.2f));
  // Dot grid print.
  for (float gx = min.x + 25; gx < max.x - 10; gx += 25) {
    for (float gy = min.y + 22; gy < max.y - 10; gy += 22) {
      dl->AddCircleFilled(c.P(gx, gy), c.S(1.1f), IM_COL32(120, 135, 145, 110));
    }
  }

  Interaction in = HitBox(c, "##xy", min, max);
  ImGuiIO& io = ImGui::GetIO();
  float x = 0.5f;
  float y = 0.5f;
  if (in.active) {
    ImVec2 a = c.P(min);
    ImVec2 b = c.P(max);
    x = ImClamp((io.MousePos.x - a.x) / (b.x - a.x), 0.0f, 1.0f);
    y = ImClamp(1.0f - (io.MousePos.y - a.y) / (b.y - a.y), 0.0f, 1.0f);
  }
  bool grids_mode = engine_->xy_mode == XY_GRIDS;
  if (grids_mode) {
    if (in.active) {
      engine_->grids_x = x;
      engine_->grids_y = y;
    }
    engine_->xy_touched = false;
    x = engine_->grids_x;
    y = engine_->grids_y;
  } else {
    engine_->xy_touched = in.active;
    engine_->xy_x = x;
    engine_->xy_y = y;
  }

  bool show_cursor = grids_mode || in.active;
  if (show_cursor) {
    ImVec2 p(min.x + x * (max.x - min.x), max.y - y * (max.y - min.y));
    ImU32 color = grids_mode && !engine_->grids_enabled ? colors::kLabelDim : colors::kAccent;
    dl->AddLine(c.P(p.x, min.y), c.P(p.x, max.y), WithAlpha(color, 70), c.S(1));
    dl->AddLine(c.P(min.x, p.y), c.P(max.x, p.y), WithAlpha(color, 70), c.S(1));
    dl->AddCircleFilled(c.P(p), c.S(10), WithAlpha(color, 80));
    dl->AddCircleFilled(c.P(p), c.S(5), color);
  }

  char text[64];
  switch (engine_->xy_mode) {
    case XY_GRIDS:
      snprintf(text, sizeof(text), "GRIDS %s  %d,%d", engine_->grids_enabled ? "ON" : "OFF",
               static_cast<int>(x * 255), static_cast<int>(y * 255));
      break;
    case XY_FX:
      snprintf(text, sizeof(text), "FX  X: DJ filter  Y: delay");
      break;
    case XY_LOOPER: {
      static const char* const kLoopNames[] = { "1/32", "1/16", "1/8", "1/4", "1/2", "1 bar" };
      snprintf(text, sizeof(text), "LOOPER  %s", in.active ? kLoopNames[Discrete(y, 6)] : "touch to loop");
      break;
    }
    case XY_MACROS:
      snprintf(text, sizeof(text), "MACROS  (Assign page)");
      break;
  }
  c.Text(min.x + 10, max.y - 18, 9, colors::kLabelDim, text, false);
}

void Faceplate::DrawScreenSection() {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  // Bezel with the screen on the left and the encoder column on the right.
  const float bezel_y1 = layout::kButtonRowY0 - 14;
  dl->AddRectFilled(c.P(layout::kRightX0, layout::kTopY0 + 3),
                    c.P(layout::kScreenX1, bezel_y1 + 3), IM_COL32(0, 0, 0, 110), c.S(10));
  dl->AddRectFilled(c.P(layout::kRightX0, layout::kTopY0),
                    c.P(layout::kScreenX1, bezel_y1), colors::kBezel, c.S(10));
  const float screen_x1 = layout::kScreenX1 - 150;
  DrawScreen(ImVec2(layout::kRightX0 + 12, layout::kTopY0 + 12),
             ImVec2(screen_x1, bezel_y1 - 12));

  // Encoder: turns change the engine of the selected track (or the preset in
  // the browser).
  const float encoder_x = (screen_x1 + layout::kScreenX1) * 0.5f;
  float before = encoder_;
  Knob(c, "##encoder", ImVec2(encoder_x, layout::kTopY0 + 58), 34, &encoder_, 0.5f,
       KNOB_ENCODER, false, 0);
  encoder_accumulator_ += encoder_ - before;
  if (encoder_ <= 0.0f || encoder_ >= 1.0f) {
    encoder_ = 0.5f;
  }
  int steps = static_cast<int>(encoder_accumulator_ / 0.04f);
  if (steps != 0) {
    encoder_accumulator_ -= steps * 0.04f;
    int track = engine_->selected_track;
    TrackSound& sound = engine_->project().sounds[track];
    if (view_ == VIEW_BROWSE) {
      browse_index_ = (browse_index_ + steps + kNumPresets * 4) % kNumPresets;
      ApplyPreset(browse_index_, &sound);
      engine_->LiveTrigger(track, 0.8f, 0);
    } else {
      int engine = Discrete(sound.params[P_ENGINE], kNumEngines);
      engine = (engine + steps + kNumEngines * 4) % kNumEngines;
      sound.params[P_ENGINE] = DiscreteToNormalized(engine, kNumEngines);
    }
  }

  if (Button(c, "##back", ImVec2(encoder_x - 30, layout::kTopY0 + 106),
             ImVec2(encoder_x + 30, layout::kTopY0 + 134), "", "", ICON_BACK,
             false, 0).pressed) {
    view_ = VIEW_MAIN;
    held_step_ = -1;
    chord_edit_ = -1;
    song_edit_ = false;
  }

  // 2x4 LEDs: playing page (top) and edited page (bottom).
  int play_page = engine_->pattern_step.load() >= 0 ? engine_->pattern_step.load() / kStepsPerPage : -1;
  for (int i = 0; i < 4; ++i) {
    float lx = encoder_x - 21 + i * 14.0f;
    dl->AddCircleFilled(c.P(lx, bezel_y1 - 26), c.S(2.8f),
                        i == play_page ? colors::kAmber : colors::kLedOff);
    dl->AddCircleFilled(c.P(lx, bezel_y1 - 14), c.S(2.8f),
                        i == view_page_ ? colors::kAccent : colors::kLedOff);
  }

  // Buttons under the screen.
  struct ScreenButton {
    const char* id;
    const char* label;
    Icon icon;
  };
  const ScreenButton kButtons[4] = {
    { "##sample", "Sample", ICON_SAMPLE },
    { "##browse", "Browse", ICON_BROWSE },
    { "##edit", "Edit", ICON_EDIT },
    { "##save", "Save", ICON_SAVE },
  };
  float w = (layout::kScreenX1 - layout::kRightX0 - 3 * layout::kGap) / 4.0f;
  for (int i = 0; i < 4; ++i) {
    float x0 = layout::kRightX0 + i * (w + layout::kGap);
    bool lit = (i == 1 && view_ == VIEW_BROWSE) || (i == 2 && view_ == VIEW_EDIT);
    if (!Button(c, kButtons[i].id, ImVec2(x0, layout::kButtonRowY0),
                ImVec2(x0 + w, layout::kButtonRowY1), kButtons[i].label, "",
                kButtons[i].icon, lit, colors::kAccent).pressed) {
      continue;
    }
    switch (i) {
      case 0:
        Toast("Sampler not emulated (Plaits only)");
        break;
      case 1:
        view_ = view_ == VIEW_BROWSE ? VIEW_MAIN : VIEW_BROWSE;
        break;
      case 2:
        view_ = view_ == VIEW_EDIT ? VIEW_MAIN : VIEW_EDIT;
        break;
      default:
        Save();
        break;
    }
  }
}

void Faceplate::DrawSoundEdit() {
  const Canvas& c = canvas_;
  int track = engine_->selected_track;
  bool track_page = page_ == PAGE_SOUND || page_ == PAGE_FILTER || page_ == PAGE_TRACK;
  Step* held = held_step_ >= 0 && track_page
      ? &pattern().tracks[track].steps[held_step_] : nullptr;

  const float column = (layout::kRightX1 - layout::kSoundX0) / 4.0f;
  for (int k = 0; k < kKnobsPerPage; ++k) {
    ImVec2 center(layout::kSoundX0 + column * (k % 4 + 0.5f),
                  k < 4 ? layout::kTopY0 + 58 : layout::kTopY0 + 132);
    bool unused;
    float* base = KnobTarget(k, &unused);
    int param = track_page ? page_ * kKnobsPerPage + k : -1;
    bool locked = held && held->has_lock(param);
    float value = locked ? held->locks[param] : *base;
    float default_value = page_param_info(page_, k).default_value;

    // What the sequencer does to this parameter: a glow while a lock is
    // playing, a marker at the value actually heard (locks, LFOs, macros),
    // and a badge when some step of the track locks it.
    KnobDecor decor;
    if (param >= 0 && param < kNumLockable) {
      uint32_t lock_mask = engine_->active_lock_mask[track].load(std::memory_order_relaxed);
      if (lock_mask & (1u << param)) {
        float flash = ImClamp(static_cast<float>(1.0 - (ImGui::GetTime() - flash_time_[track]) / 0.25),
                              0.0f, 1.0f);
        decor.halo = WithAlpha(colors::kAccent, static_cast<int>(90 + 150 * flash));
      }
      decor.marker = engine_->effective_param[track][param].load(std::memory_order_relaxed);
      decor.marker_color = (lock_mask & (1u << param)) ? colors::kAccent : colors::kLockMod;
      int length = pattern().track_length(track);
      for (int s = 0; s < length && !decor.automated; ++s) {
        decor.automated = pattern().tracks[track].steps[s].has_lock(param);
      }
    }

    char id[16];
    snprintf(id, sizeof(id), "##knob%d", k);
    Interaction in;
    bool changed = Knob(c, id, center, 22, &value, default_value, KNOB_SILVER, true,
                        locked ? colors::kAccent : colors::kLedWhite, &in,
                        held ? nullptr : &decor);
    if (held && in.double_clicked) {
      held->locks[param] = NAN;
    } else if (changed) {
      if (held) {
        held->locks[param] = value;
      } else {
        *base = value;
        // Motion recording: knob moves become locks on the playing step.
        int step = engine_->track_step[track].load();
        if (engine_->recording && engine_->playing() && track_page && step >= 0) {
          pattern().tracks[track].steps[step].locks[param] = value;
        }
      }
    }
    // Short parameter name under each knob.
    c.Text(center.x, center.y + 38, 8.5f, colors::kLabelDim,
           page_param_info(page_, k).short_name, true, true);
    if (in.hovered || in.active) {
      std::string tip = std::string(page_param_info(page_, k).name) + ": " +
          FormatValue(page_param_info(page_, k).format, value);
      ImGui::SetTooltip("%s", tip.c_str());
    }
  }

  const Icon kPageIcons[PAGE_LAST] = {
    ICON_SOUND, ICON_FILTER, ICON_TRACK, ICON_MASTER, ICON_MODS, ICON_ASSIGN
  };
  float w = (layout::kRightX1 - layout::kSoundX0 - (PAGE_LAST - 1) * layout::kGap) / PAGE_LAST;
  for (int i = 0; i < PAGE_LAST; ++i) {
    Page page = static_cast<Page>(i);
    float x0 = layout::kSoundX0 + i * (w + layout::kGap);
    char id[16];
    snprintf(id, sizeof(id), "##page_%d", i);
    if (Button(c, id, ImVec2(x0, layout::kButtonRowY0), ImVec2(x0 + w, layout::kButtonRowY1),
               page_name(page), "", kPageIcons[i], page_ == page, colors::kAccent).pressed) {
      page_ = page;
    }
  }
}

void Faceplate::DrawSequencer() {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  Pattern& p = pattern();
  int track = engine_->selected_track;
  double now = ImGui::GetTime();
  bool blink = std::fmod(now, 0.5) < 0.25;

  // Function row: one button per step column.
  auto function_button = [&](int column, const char* id, const char* label, bool lit,
                             ImU32 color) {
    float x0 = layout::StepX(column);
    return Button(c, id, ImVec2(x0, layout::kFunctionRowY0),
                  ImVec2(x0 + layout::kStepWidth, layout::kFunctionRowY1), label, "",
                  ICON_NONE, lit, color);
  };

  // Group 1: what the step buttons do.
  if (function_button(0, "##patterns", "Patterns", patterns_mode_, colors::kAccent).pressed) {
    patterns_mode_ = !patterns_mode_;
    last_step_mode_ = false;
    song_edit_ = false;
  }
  if (function_button(1, "##steps", "Steps", !patterns_mode_ && !last_step_mode_ && !song_edit_,
                      colors::kAccent).pressed) {
    patterns_mode_ = false;
    last_step_mode_ = false;
    song_edit_ = false;
  }
  if (function_button(2, "##notes", "Notes", notes_mode_, colors::kAccent).pressed) {
    notes_mode_ = !notes_mode_;
  }
  if (function_button(3, "##last", "Last Step", last_step_mode_, colors::kAccent).pressed) {
    last_step_mode_ = !last_step_mode_;
    patterns_mode_ = false;
    song_edit_ = false;
  }

  // Group 2: step pages.
  int play_page = engine_->pattern_step.load() >= 0 ? engine_->pattern_step.load() / kStepsPerPage : -1;
  const char* const kPageLabels[kNumPages] = { "A|1", "B|2", "C|3", "D|4" };
  for (int i = 0; i < kNumPages; ++i) {
    char id[16];
    snprintf(id, sizeof(id), "##page%d", i);
    bool in_pattern = i * kStepsPerPage < std::max(p.length, p.track_length(track)) ||
        last_step_mode_;
    ImU32 color = i == view_page_ ? colors::kAccent
        : (i == play_page ? colors::kAmber : colors::kLabelDim);
    if (function_button(4 + i, id, kPageLabels[i],
                        in_pattern && (i == view_page_ || i == play_page),
                        color).pressed) {
      view_page_ = i;
    }
    if (!in_pattern) {
      float x0 = layout::StepX(4 + i);
      dl->AddRectFilled(c.P(x0 + 2, layout::kFunctionRowY0 + 2),
                        c.P(x0 + layout::kStepWidth - 2, layout::kFunctionRowY1 - 3),
                        IM_COL32(0, 0, 0, 90), c.S(4));
    }
  }

  // Group 3: song, polyrhythm and pad behavior.
  // Song: first press edits the chain (pattern buttons append to it), second
  // press plays it.
  bool song_lit = song_edit_ ? blink : engine_->song_mode();
  if (function_button(8, "##song", "Song", song_lit,
                      song_edit_ ? colors::kAmber : colors::kAccent).pressed) {
    if (!song_edit_) {
      song_edit_ = true;
      song_edit_fresh_ = true;
      patterns_mode_ = false;
      last_step_mode_ = false;
      Toast("Song: press patterns in order");
    } else {
      song_edit_ = false;
      engine_->SetSongMode(true);
      int length = engine_->project().song_length;
      char message[48];
      snprintf(message, sizeof(message), length ? "Song: %d patterns" : "Song empty", length);
      Toast(message);
    }
  }
  if (function_button(9, "##poly", "Poly", p.poly, colors::kAccent).pressed) {
    p.poly = !p.poly;
    Toast(p.poly ? "Poly: tracks cycle on their own length" : "Poly off: tracks restart with the pattern");
  }
  if (function_button(10, "##select", "Select", select_mode_, colors::kAccent).pressed) {
    select_mode_ = !select_mode_;
    mute_mode_ = false;
  }
  if (function_button(11, "##mute", "Mute", mute_mode_, colors::kAccent).pressed) {
    mute_mode_ = !mute_mode_;
    select_mode_ = false;
  }

  // Group 4: editing.
  if (function_button(12, "##copy", "Copy", false, 0).pressed) {
    Copy();
  }
  if (function_button(13, "##paste", "Paste", false, 0).pressed) {
    Paste();
  }
  if (function_button(14, "##erase", "Erase", false, 0).pressed) {
    Erase();
  }
  if (function_button(15, "##undo", "Undo", false, 0).pressed) {
    Undo();
  }

  // The 16 step buttons.
  int playing_step = engine_->track_step[track].load();
  int track_length = p.track_length(track);
  for (int i = 0; i < kStepsPerPage; ++i) {
    float x0 = layout::StepX(i);
    ImVec2 min(x0, layout::kStepRowY0);
    ImVec2 max(x0 + layout::kStepWidth, layout::kStepRowY1);
    char id[16];
    snprintf(id, sizeof(id), "##step%d", i);
    char face[16];
    bool lit = false;
    ImU32 color = colors::kLedWhite;

    if (song_edit_) {
      Project& project = engine_->project();
      snprintf(face, sizeof(face), "%d", i + 1);
      int uses = 0;
      for (int s = 0; s < project.song_length; ++s) {
        uses += project.song[s] == i;
      }
      if (i == engine_->current_pattern() && engine_->playing()) {
        lit = true;
        color = colors::kAmber;
      } else if (uses > 0) {
        lit = true;
        color = colors::kAccent;
      } else if (!project.patterns[i].IsEmpty()) {
        lit = true;
        color = IM_COL32(120, 130, 138, 255);
      }
      Interaction in = Button(c, id, min, max, face, "", ICON_NONE, lit, color);
      if (uses > 1) {
        char count[16];
        snprintf(count, sizeof(count), "x%d", uses);
        c.Text(max.x - 12, min.y + 9, 8, colors::kAccent, count, true, true);
      }
      if (in.pressed) {
        if (song_edit_fresh_) {
          // Like holding Song on the Arturia machines: a new chain.
          project.song_length = 0;
          song_edit_fresh_ = false;
        }
        if (project.song_length < kMaxSongLength) {
          project.song[project.song_length++] = i;
        } else {
          Toast("Song full (64 patterns)");
        }
      }
      continue;
    }

    if (patterns_mode_) {
      snprintf(face, sizeof(face), "%d", i + 1);
      bool empty = engine_->project().patterns[i].IsEmpty();
      if (i == engine_->current_pattern()) {
        lit = true;
        color = colors::kAccent;
      } else if (i == engine_->queued_pattern()) {
        lit = blink;
        color = colors::kAmber;
      } else if (!empty) {
        lit = true;
        color = IM_COL32(120, 130, 138, 255);
      }
      Interaction in = Button(c, id, min, max, face, "", ICON_NONE, lit, color);
      if (in.pressed) {
        engine_->SelectPattern(i);
      }
      continue;
    }

    int index = step_index(i);
    const Step& step = p.tracks[track].steps[index];
    bool in_range = index < track_length;

    if (last_step_mode_) {
      // DrumBrute-style: press the step that ends the track.
      snprintf(face, sizeof(face), "%d", index + 1);
      lit = in_range;
      color = index == track_length - 1 ? colors::kAccent : IM_COL32(150, 158, 165, 255);
      Interaction in = Button(c, id, min, max, face, "", ICON_NONE, lit, color);
      if (in.pressed) {
        PushUndo();
        TrackSequence& sequence = p.tracks[track];
        // Pressing the current last step again follows the pattern length.
        sequence.length = sequence.length == index + 1 ? 0 : index + 1;
        char message[64];
        snprintf(message, sizeof(message), "Track %c: %d steps%s", 'A' + track,
                 p.track_length(track), sequence.length == 0 ? " (pattern)" : "");
        Toast(message);
      }
      continue;
    }
    if (notes_mode_ && step.on) {
      snprintf(face, sizeof(face), "%+d", step.note);
    } else {
      snprintf(face, sizeof(face), "%d", i + 1);
    }
    if (index == playing_step && engine_->playing()) {
      lit = true;
      color = colors::kAmber;
    } else if (step.on) {
      lit = true;
      color = WithAlpha(colors::kLedWhite, static_cast<int>(120 + 135 * step.velocity));
    }
    if (!in_range) {
      lit = false;
    }
    Interaction in = Button(c, id, min, max, face, "", ICON_NONE, lit, color);
    if (in.pressed && in_range) {
      PushUndo();
      Step& s = p.tracks[track].steps[index];
      s.on = !s.on;
      if (s.on) {
        s.velocity = ImGui::GetIO().KeyShift ? 1.0f : 0.8f;
      }
    }
    if (in.right_clicked && in_range) {
      held_step_ = held_step_ == index ? -1 : index;
    }
    if (!in_range) {
      dl->AddRectFilled(c.P(min.x + 2, min.y + 2), c.P(max.x - 2, max.y - 3),
                        IM_COL32(0, 0, 0, 110), c.S(4));
    }
    if (held_step_ == index) {
      dl->AddRect(c.P(min.x - 3, min.y - 3), c.P(max.x + 3, max.y + 3),
                  colors::kAccent, c.S(6), 0, c.S(2));
    }
    if (step.has_any_lock()) {
      dl->AddCircleFilled(c.P(max.x - 7, min.y + 7), c.S(2.5f), colors::kAccent);
    }
    if (step.ratchets > 1 && step.on) {
      for (int r = 0; r < step.ratchets; ++r) {
        dl->AddCircleFilled(c.P(min.x + 7 + r * 5.0f, max.y - 8), c.S(1.5f), colors::kLabel);
      }
    }
  }
}

void Faceplate::DrawPads() {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  Project& project = engine_->project();
  double now = ImGui::GetTime();
  const ImU32 kPadInk = IM_COL32(38, 44, 50, 255);

  for (int t = 0; t < kNumTracks; ++t) {
    float x0 = layout::PadX(t);
    ImVec2 min(x0, layout::kPadRow1Y0);
    ImVec2 max(x0 + layout::kPadWidth, layout::kPadRow1Y1);
    char id[16];
    snprintf(id, sizeof(id), "##pad%d", t);
    Interaction in = HitBox(c, id, min, max);
    if (in.pressed) {
      TrackPadDown(t);
    }
    if (in.released) {
      TrackPadUp(t);
    }

    float flash = static_cast<float>(1.0 - (now - flash_time_[t]) / 0.15);
    flash = ImClamp(flash, 0.0f, 1.0f);
    bool selected = t == engine_->selected_track;
    bool muted = engine_->muted[t];
    bool in_chord = chord_edit_ >= 0 && (project.chords[chord_edit_] & (1 << t));

    float split = min.y + (max.y - min.y) * 0.5f;
    dl->AddRectFilled(c.P(min.x, min.y + 4), c.P(max.x, max.y + 4), IM_COL32(0, 0, 0, 120), c.S(8));
    // Lower light part, lit by the pad's LED on each hit.
    ImU32 face = muted ? IM_COL32(128, 134, 136, 255) : colors::kPadFace;
    if (flash > 0.0f || in_chord) {
      float f = in_chord ? 1.0f : flash;
      face = ImGui::ColorConvertFloat4ToU32(ImLerp(
          ImGui::ColorConvertU32ToFloat4(face),
          ImGui::ColorConvertU32ToFloat4(colors::kAccent), f * 0.8f));
    }
    dl->AddRectFilled(c.P(min), c.P(max), face, c.S(8));
    dl->AddRectFilled(c.P(min), c.P(max.x, split), colors::kPadDark, c.S(8),
                      ImDrawFlags_RoundCornersTop);
    // Name, engine and level meter in the dark upper part.
    const TrackSound& sound = project.sounds[t];
    c.Text(min.x + 10, min.y + 10, 10.5f, colors::kLedWhite, sound.name.c_str(), false, true);
    c.Text(min.x + 10, min.y + 27, 8.5f, colors::kLabelDim,
           kEngineNames[Discrete(sound.params[P_ENGINE], kNumEngines)], false);
    float level = std::min(1.0f, engine_->track_peak[t].load() * 1.6f);
    dl->AddRectFilled(c.P(min.x + 10, split - 14), c.P(max.x - 10, split - 10),
                      IM_COL32(52, 60, 68, 255), c.S(2));
    dl->AddRectFilled(c.P(min.x + 10, split - 14),
                      c.P(min.x + 10 + (max.x - min.x - 20) * level, split - 10),
                      colors::kAccent, c.S(2));
    char letter[2] = { static_cast<char>('A' + t), 0 };
    c.Text(min.x + 10, max.y - 26, 15, kPadInk, letter, false, true);
    if (muted) {
      c.Text(max.x - 36, max.y - 22, 9, kPadInk, "MUTE", false, true);
    }
    if (selected) {
      dl->AddRect(c.P(min.x - 4, min.y - 4), c.P(max.x + 4, max.y + 4), colors::kAccent,
                  c.S(11), 0, c.S(2.5f));
    }
  }

  for (int i = 0; i < 8; ++i) {
    float x0 = layout::PadX(i);
    ImVec2 min(x0, layout::kPadRow2Y0);
    ImVec2 max(x0 + layout::kPadWidth, layout::kPadRow2Y1);
    char id[16];
    snprintf(id, sizeof(id), "##lower%d", i);
    Interaction in = HitBox(c, id, min, max);
    if (in.pressed) {
      LowerPadDown(i);
    }
    if (in.released) {
      LowerPadUp(i);
    }
    if (in.right_clicked && pad_mode_ == PADS_CHORDS) {
      chord_edit_ = chord_edit_ == i ? -1 : i;
    }

    bool held = lower_pad_held_[i] || in.active;
    ImU32 face = colors::kPadFace;
    if (held || chord_edit_ == i) {
      face = ImGui::ColorConvertFloat4ToU32(ImLerp(
          ImGui::ColorConvertU32ToFloat4(face),
          ImGui::ColorConvertU32ToFloat4(colors::kAccent), chord_edit_ == i ? 0.7f : 0.5f));
    }
    dl->AddRectFilled(c.P(min.x, min.y + 4), c.P(max.x, max.y + 4), IM_COL32(0, 0, 0, 120), c.S(8));
    dl->AddRectFilled(c.P(min), c.P(max), face, c.S(8));
    char number[4];
    snprintf(number, sizeof(number), "%d", i + 1);
    c.Text(min.x + 10, max.y - 26, 15, kPadInk, number, false, true);

    std::string function;
    switch (pad_mode_) {
      case PADS_SOUNDS: function = preset_name(preset_bank_ * 8 + i); break;
      case PADS_REPEATS: function = kRepeatNames[i]; break;
      case PADS_CHORDS: function = ChordName(project.chords[i]); break;
      case PADS_KEYS: {
        char text[8];
        snprintf(text, sizeof(text), "+%d", kScaleNotes[i]);
        function = text;
        break;
      }
    }
    c.Text((min.x + max.x) * 0.5f, min.y + 40, 10, IM_COL32(95, 104, 110, 255),
           function.c_str(), true, true);
  }
}

// --- Screen ----------------------------------------------------------------------

void Faceplate::DrawScreen(ImVec2 min, ImVec2 max) {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  dl->AddRectFilled(c.P(min), c.P(max), colors::kScreen, c.S(2));
  dl->PushClipRect(c.P(min), c.P(max), true);
  if (chord_edit_ >= 0) {
    char title[48];
    snprintf(title, sizeof(title), "CHORD PAD %d", chord_edit_ + 1);
    c.Text(min.x + 8, min.y + 6, 11, colors::kScreenAccent, title, false, true);
    c.Text(min.x + 8, min.y + 30, 18, colors::kScreenText,
           ChordName(engine_->project().chords[chord_edit_]).c_str(), false, true);
    c.Text(min.x + 8, min.y + 70, 9, colors::kScreenDim, "Tap pads A-H to add or remove", false);
    c.Text(min.x + 8, min.y + 84, 9, colors::kScreenDim, "tracks. Back button to finish.", false);
  } else if (song_edit_) {
    DrawSongView(min, max);
  } else if (held_step_ >= 0) {
    DrawStepView(min, max);
  } else if (view_ == VIEW_EDIT) {
    DrawEditView(min, max);
  } else if (view_ == VIEW_BROWSE) {
    DrawBrowseView(min, max);
  } else {
    DrawMainView(min, max);
  }
  if (ImGui::GetTime() - toast_time_ < 1.6) {
    dl->AddRectFilled(c.P(min.x, max.y - 18), c.P(max), WithAlpha(colors::kAccent, 235));
    c.Text(min.x + 6, max.y - 15, 9.5f, IM_COL32(8, 30, 26, 255), toast_.c_str(), false, true);
  }
  dl->PopClipRect();
}

void Faceplate::DrawMainView(ImVec2 min, ImVec2 max) {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  Project& project = engine_->project();
  int track = engine_->selected_track;
  const TrackSound& sound = project.sounds[track];

  char text[96];
  snprintf(text, sizeof(text), "P%02d", engine_->current_pattern() + 1);
  c.Text(min.x + 6, min.y + 3, 13, colors::kScreenAccent, text, false, true);
  if (engine_->song_mode()) {
    snprintf(text, sizeof(text), "SONG %d/%d",
             std::max(0, engine_->song_position()) + 1, engine_->project().song_length);
    c.Text(min.x + 42, min.y + 3, 13, colors::kScreenDim, text, false, true);
  } else if (engine_->queued_pattern() >= 0) {
    snprintf(text, sizeof(text), "> P%02d", engine_->queued_pattern() + 1);
    c.Text(min.x + 42, min.y + 3, 13, colors::kScreenDim, text, false, true);
  }
  snprintf(text, sizeof(text), "%s%s  %.1f BPM",
           engine_->recording ? "REC " : "",
           engine_->playing() ? ">" : "#", project.bpm);
  c.Text(max.x - 112, min.y + 4, 11.5f, engine_->recording ? colors::kRed : colors::kScreenText,
         text, false, true);

  snprintf(text, sizeof(text), "%c  %s", 'A' + track, sound.name.c_str());
  c.Text(min.x + 6, min.y + 20, 15, colors::kScreenText, text, false, true);
  const Pattern& p = project.patterns[engine_->current_pattern()];
  snprintf(text, sizeof(text), "%s  |  %s  |  %d/%d steps%s", page_name(page_),
           kEngineNames[Discrete(sound.params[P_ENGINE], kNumEngines)],
           p.track_length(track), p.length, p.poly ? "  POLY" : "");
  c.Text(min.x + 6, min.y + 40, 10, colors::kScreenDim, text, false);

  // The 8 parameters of the current page.
  float cell = (max.x - min.x - 8) / 4.0f;
  for (int k = 0; k < kKnobsPerPage; ++k) {
    bool unused;
    float value = *KnobTarget(k, &unused);
    const ParamInfo& info = page_param_info(page_, k);
    float x = min.x + 4 + (k % 4) * cell;
    float y = min.y + 56 + (k / 4) * 31.0f;
    c.Text(x + 2, y, 9, colors::kScreenDim, info.short_name, false);
    c.Text(x + 2, y + 10, 11, colors::kScreenText, FormatValue(info.format, value).c_str(), false, true);
    dl->AddRectFilled(c.P(x + 2, y + 25), c.P(x + cell - 6, y + 27), IM_COL32(40, 55, 60, 255));
    dl->AddRectFilled(c.P(x + 2, y + 25), c.P(x + 2 + (cell - 8) * value, y + 27),
                      colors::kScreenAccent);
  }

  DrawScope(ImVec2(min.x + 6, min.y + 121), ImVec2(max.x - 56, max.y - 4));
  snprintf(text, sizeof(text), "CPU %d%%", static_cast<int>(engine_->cpu_load.load() * 100.0f));
  c.Text(max.x - 50, max.y - 30, 8, colors::kScreenDim, text, false);
  float level = std::min(1.0f, engine_->master_peak.load());
  dl->AddRectFilled(c.P(max.x - 50, max.y - 14), c.P(max.x - 6, max.y - 10), IM_COL32(40, 55, 60, 255));
  dl->AddRectFilled(c.P(max.x - 50, max.y - 14), c.P(max.x - 50 + 44 * level, max.y - 10),
                    level > 0.95f ? colors::kRed : colors::kScreenText);
}

void Faceplate::DrawScope(ImVec2 min, ImVec2 max) {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  dl->AddRect(c.P(min), c.P(max), IM_COL32(30, 45, 50, 255));
  const int kPoints = 200;
  ImVec2 points[kPoints];
  int write = engine_->scope_write.load();
  float mid = (min.y + max.y) * 0.5f;
  float half = (max.y - min.y) * 0.5f;
  for (int i = 0; i < kPoints; ++i) {
    int index = (write + i * (kScopeSize / kPoints)) % kScopeSize;
    float s = ImClamp(engine_->scope[index].load(std::memory_order_relaxed), -1.0f, 1.0f);
    points[i] = c.P(min.x + (max.x - min.x) * i / (kPoints - 1), mid - s * half);
  }
  dl->AddPolyline(points, kPoints, colors::kScreenText, 0, c.S(1.0f));
}

void Faceplate::DrawSongView(ImVec2 min, ImVec2 max) {
  const Canvas& c = canvas_;
  ImDrawList* dl = c.draw_list;
  const Project& project = engine_->project();
  char text[64];
  snprintf(text, sizeof(text), "SONG  |  %d pattern%s", project.song_length,
           project.song_length == 1 ? "" : "s");
  c.Text(min.x + 6, min.y + 4, 10.5f, colors::kScreenAccent, text, false, true);
  c.Text(min.x + 6, min.y + 20, 8.5f, colors::kScreenDim,
         song_edit_fresh_ ? "Press patterns to start a new chain"
                          : "Erase = clear, Undo = remove last, Song = play",
         false);

  // The chain, 12 entries per row; the playing entry is highlighted.
  const int kPerRow = 12;
  float cell = (max.x - min.x - 12) / kPerRow;
  int playing = engine_->song_mode() && engine_->playing() ? engine_->song_position() : -1;
  for (int s = 0; s < project.song_length && s < kPerRow * 5; ++s) {
    float x = min.x + 6 + (s % kPerRow) * cell;
    float y = min.y + 36 + (s / kPerRow) * 20.0f;
    bool current = s == playing;
    dl->AddRectFilled(c.P(x, y), c.P(x + cell - 3, y + 16),
                      current ? colors::kScreenAccent : IM_COL32(30, 50, 56, 255), c.S(2));
    snprintf(text, sizeof(text), "%d", project.song[s] + 1);
    c.Text(x + (cell - 3) * 0.5f, y + 8, 9.5f,
           current ? IM_COL32(8, 30, 26, 255) : colors::kScreenText, text, true, true);
  }
}

void Faceplate::DrawStepView(ImVec2 min, ImVec2 max) {
  const Canvas& c = canvas_;
  int track = engine_->selected_track;
  Step& step = pattern().tracks[track].steps[held_step_];
  char text[64];
  snprintf(text, sizeof(text), "STEP %02d  |  %c %s", held_step_ + 1, 'A' + track,
           engine_->project().sounds[track].name.c_str());
  c.Text(min.x + 6, min.y + 4, 10.5f, colors::kScreenAccent, text, false, true);

  float width = c.S(max.x - min.x - 16);
  ImGui::SetCursorScreenPos(c.P(min.x + 8, min.y + 22));
  ImGui::PushItemWidth(width);
  if (ImGui::Checkbox("Active", &step.on)) { }
  ImGui::SetCursorScreenPos(c.P(min.x + 8, min.y + 42));
  ImGui::SliderFloat("##vel", &step.velocity, 0.0f, 1.0f, "Velocity %.2f");
  ImGui::SetCursorScreenPos(c.P(min.x + 8, min.y + 60));
  float probability = step.probability * 100.0f;
  if (ImGui::SliderFloat("##prob", &probability, 0.0f, 100.0f, "Probability %.0f%%")) {
    step.probability = probability / 100.0f;
  }
  ImGui::SetCursorScreenPos(c.P(min.x + 8, min.y + 78));
  ImGui::SliderInt("##rat", &step.ratchets, 1, kMaxRatchets, "Ratchets %d");
  ImGui::SetCursorScreenPos(c.P(min.x + 8, min.y + 96));
  ImGui::SliderInt("##note", &step.note, -24, 24, "Note %+d");
  ImGui::PopItemWidth();

  std::string locks;
  for (int p = 0; p < kNumLockable; ++p) {
    if (step.has_lock(p)) {
      locks += std::string(locks.empty() ? "" : ", ") + track_param_info(p).short_name;
    }
  }
  c.Text(min.x + 8, min.y + 116, 8.5f, colors::kScreenDim,
         locks.empty() ? "No locks: turn a knob to lock it" : ("Locks: " + locks).c_str(), false);
  if (!locks.empty()) {
    ImGui::SetCursorScreenPos(c.P(max.x - 72, min.y + 128));
    if (ImGui::SmallButton("Clear locks")) {
      step.ClearLocks();
    }
  }
}

void Faceplate::DrawEditView(ImVec2 min, ImVec2 max) {
  const Canvas& c = canvas_;
  Project& project = engine_->project();
  Pattern& p = pattern();
  int track = engine_->selected_track;
  char text[48];
  snprintf(text, sizeof(text), "EDIT  |  PATTERN %02d", engine_->current_pattern() + 1);
  c.Text(min.x + 6, min.y + 4, 10.5f, colors::kScreenAccent, text, false, true);

  float column = c.S((max.x - min.x - 20) * 0.5f);
  float x0 = min.x + 8;
  float x1 = min.x + 12 + (max.x - min.x - 20) * 0.5f;
  ImGui::PushItemWidth(column);
  ImGui::SetCursorScreenPos(c.P(x0, min.y + 22));
  ImGui::SliderInt("##len", &p.length, 1, kMaxSteps, "Length %d");
  ImGui::SetCursorScreenPos(c.P(x1, min.y + 22));
  float swing = p.swing * 100.0f;
  if (ImGui::SliderFloat("##swing", &swing, 50.0f, 75.0f, "Swing %.0f%%")) {
    p.swing = swing / 100.0f;
  }
  ImGui::SetCursorScreenPos(c.P(x0, min.y + 42));
  snprintf(text, sizeof(text), "Trk %c len %%d", 'A' + track);
  ImGui::SliderInt("##tlen", &p.tracks[track].length, 0, kMaxSteps,
                   p.tracks[track].length == 0 ? "Trk len = pattern" : text);
  ImGui::SetCursorScreenPos(c.P(x1, min.y + 42));
  ImGui::SliderFloat("##bpm", &project.bpm, 30.0f, 300.0f, "%.1f BPM");
  ImGui::SetCursorScreenPos(c.P(x0, min.y + 62));
  bool song = engine_->song_mode();
  if (ImGui::Checkbox("Play song", &song)) {
    engine_->SetSongMode(song);
  }
  ImGui::SetCursorScreenPos(c.P(x1, min.y + 62));
  ImGui::Checkbox("Metronome", &engine_->metronome);
  ImGui::PopItemWidth();

  ImGui::SetCursorScreenPos(c.P(x0, min.y + 86));
  if (ImGui::Button("Bake Grids > A-C")) {
    PushUndo();
    engine_->BakeGrids();
    Toast("Grids pattern written to A-C");
  }
  ImGui::SetCursorScreenPos(c.P(x1, min.y + 86));
  if (ImGui::Button("Init sound")) {
    project.sounds[track].Reset();
    Toast("Sound initialized");
  }
  ImGui::SetCursorScreenPos(c.P(x0, min.y + 108));
  if (ImGui::Button("Demo pattern")) {
    PushUndo();
    MakeDemoPattern(&p);
    Toast("Demo pattern");
  }
  ImGui::SetCursorScreenPos(c.P(x1, min.y + 108));
  if (ImGui::Button("Reload saved")) {
    std::string error;
    if (LoadProject(project_path_, &project, &error)) {
      Toast("Project reloaded");
    } else {
      Toast(error);
    }
  }
}

void Faceplate::DrawBrowseView(ImVec2 min, ImVec2 max) {
  const Canvas& c = canvas_;
  int track = engine_->selected_track;
  char text[48];
  snprintf(text, sizeof(text), "SOUNDS  >  TRACK %c", 'A' + track);
  c.Text(min.x + 6, min.y + 4, 10.5f, colors::kScreenAccent, text, false, true);
  float row = (max.y - min.y - 24) / 8.0f;
  float column = (max.x - min.x - 12) * 0.5f;
  for (int i = 0; i < kNumPresets; ++i) {
    ImVec2 a(min.x + 6 + (i / 8) * column, min.y + 20 + (i % 8) * row);
    ImVec2 b(a.x + column - 4, a.y + row - 1);
    char id[16];
    snprintf(id, sizeof(id), "##preset%d", i);
    Interaction in = HitBox(c, id, a, b);
    bool current = engine_->project().sounds[track].name == preset_name(i);
    if (current || in.hovered) {
      c.draw_list->AddRectFilled(c.P(a), c.P(b),
                                 current ? WithAlpha(colors::kAccent, 90) : IM_COL32(60, 80, 90, 120));
    }
    snprintf(text, sizeof(text), "%2d  %s", i + 1, preset_name(i));
    c.Text(a.x + 4, a.y + 1, 9, colors::kScreenText, text, false);
    if (in.pressed) {
      ApplyPreset(i, &engine_->project().sounds[track]);
      engine_->LiveTrigger(track, 0.8f, 0);
    }
  }
}

// --- Keyboard ----------------------------------------------------------------------

void Faceplate::HandleKeyboard() {
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantTextInput) {
    return;
  }
  if (io.KeyCtrl) {
    if (ImGui::IsKeyPressed(ImGuiKey_S, false)) Save();
    if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) Undo();
    if (ImGui::IsKeyPressed(ImGuiKey_C, false)) Copy();
    if (ImGui::IsKeyPressed(ImGuiKey_V, false)) Paste();
    return;
  }
  for (int t = 0; t < kNumTracks; ++t) {
    if (ImGui::IsKeyPressed(kTrackPadKeys[t], false)) TrackPadDown(t);
    if (ImGui::IsKeyReleased(kTrackPadKeys[t])) TrackPadUp(t);
  }
  for (int i = 0; i < 8; ++i) {
    if (ImGui::IsKeyPressed(kLowerPadKeys[i], false)) LowerPadDown(i);
    if (ImGui::IsKeyReleased(kLowerPadKeys[i])) LowerPadUp(i);
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) TogglePlay();
  if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) engine_->recording = !engine_->recording;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    last_step_mode_ = false;
    song_edit_ = false;
    view_ = VIEW_MAIN;
    held_step_ = -1;
    chord_edit_ = -1;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) view_page_ = std::max(0, view_page_ - 1);
  if (ImGui::IsKeyPressed(ImGuiKey_X, false)) view_page_ = std::min(kNumPages - 1, view_page_ + 1);
  if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) Erase();
  if (ImGui::IsKeyPressed(ImGuiKey_L, false)) last_step_mode_ = !last_step_mode_;
  if (ImGui::IsKeyPressed(ImGuiKey_P, false)) pattern().poly = !pattern().poly;
  const ImGuiKey kPageKeys[PAGE_LAST] = {
    ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5, ImGuiKey_6
  };
  for (int i = 0; i < PAGE_LAST; ++i) {
    if (ImGui::IsKeyPressed(kPageKeys[i], false)) page_ = static_cast<Page>(i);
  }
}

}  // namespace drumfreake
