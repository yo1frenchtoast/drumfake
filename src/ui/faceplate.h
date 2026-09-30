// The Drumf(re)ake front panel: layout, controls, screen and keyboard shortcuts.

#ifndef DRUMFREAKE_UI_FACEPLATE_H_
#define DRUMFREAKE_UI_FACEPLATE_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/engine.h"
#include "core/project_io.h"
#include "ui/widgets.h"

namespace drumfreake {

class Faceplate {
 public:
  void Init(Engine* engine, const std::string& project_path,
            ImFont* font, ImFont* bold);

  // Draws the whole panel in the current ImGui window. Takes the engine lock.
  void Draw();

  // Touchscreen fingers, in display coordinates. Each finger landing on a
  // pad, a knob or the XY pad keeps controlling it until it lifts, so several
  // can be played at once; one other finger at a time drives ImGui like a
  // mouse (buttons, screen). These take the engine lock.
  void FingerDown(uint64_t finger, ImVec2 position);
  void FingerMove(uint64_t finger, ImVec2 position);
  void FingerUp(uint64_t finger);

 private:
  enum PadMode { PADS_SOUNDS, PADS_REPEATS, PADS_CHORDS, PADS_KEYS };
  enum ScreenView { VIEW_MAIN, VIEW_EDIT, VIEW_BROWSE, VIEW_MIDI, VIEW_MARBLES, VIEW_KITS };
  enum ClipboardKind { CLIP_NONE, CLIP_STEP, CLIP_TRACK, CLIP_PATTERN };

  void DrawChrome();
  void DrawTopLeft();
  void DrawXySection();
  void DrawScreenSection();
  void DrawSoundEdit();
  void DrawSequencer();
  void DrawPads();
  void DrawShiftStep(int step, ImVec2 min, ImVec2 max);
  void DrawScreen(ImVec2 min, ImVec2 max);
  void DrawMainView(ImVec2 min, ImVec2 max);
  void DrawSongView(ImVec2 min, ImVec2 max);
  void DrawKeysView(ImVec2 min, ImVec2 max);
  void DrawSoundKnobs();
  void DrawKeyKnobs();
  void DrawPageButtons();
  ImVec2 SoundKnobCenter(int knob) const;
  void DrawStepView(ImVec2 min, ImVec2 max);
  void DrawEditView(ImVec2 min, ImVec2 max);
  void DrawBrowseView(ImVec2 min, ImVec2 max);
  void DrawMidiView(ImVec2 min, ImVec2 max);
  void DrawMarblesView(ImVec2 min, ImVec2 max);
  void DrawKitsView(ImVec2 min, ImVec2 max);
  void RefreshKits();
  void DrawScope(ImVec2 min, ImVec2 max);
  void HandleKeyboard();

  // Actions shared by the mouse and the keyboard.
  void TogglePlay();
  void TrackPadDown(int track);
  void TrackPadUp(int track);
  void LowerPadDown(int pad);
  void LowerPadUp(int pad);
  void UpdateRepeat();
  void Tap();
  void Copy();
  void Paste();
  // Copy held + two page buttons: repeats pages 1 to `source` up to
  // `destination` (or copies one page onto an earlier one).
  void CopyPages(int source, int destination);
  void Erase();
  void Undo();
  void PushUndo();
  void Save();
  void Toast(const std::string& message);

  float* KnobTarget(int knob, bool* is_lock);
  // Keys mode: offset of pad 1-8 from the track pitch, the track pitch as a
  // MIDI note, and changes to the root (0), scale (1) or octave (2).
  int KeyNote(int pad);
  int TrackNote(int track);
  void ChangeKey(int field, int delta);
  Pattern& pattern();
  int step_index(int button) const { return view_page_ * kStepsPerPage + button; }

  struct Finger {
    TouchTarget target;
    ImVec2 last;        // Panel coordinates.
    double down_time;
    bool long_press_done;
  };

  ImVec2 ToPanel(ImVec2 display) const;
  void SetXyFromTouch(const TouchTarget& target, ImVec2 panel);
  void UpdateLongPresses();

  Engine* engine_ = nullptr;
  Canvas canvas_;
  std::string project_path_;

  Page page_ = PAGE_SOUND;
  PadMode pad_mode_ = PADS_CHORDS;
  ScreenView view_ = VIEW_MAIN;
  // Kit library view.
  std::vector<KitFile> kit_files_;
  int kit_selected_ = -1;
  int kit_page_ = 0;
  bool kit_all_patterns_ = false;
  double kit_delete_armed_ = -10.0;
  int view_page_ = 0;
  bool patterns_mode_ = false;
  bool notes_mode_ = false;
  bool song_edit_ = false;
  bool song_edit_fresh_ = false;  // The next pattern pressed starts a new chain.
  bool keys_page_ = false;  // Sound Edit knobs and screen show the Keys settings.
  int keys_field_ = 0;      // Field the encoder changes: root, scale, octave.
  bool mute_mode_ = false;
  bool solo_mode_ = false;
  int held_step_ = -1;
  int preset_bank_ = 0;
  int chord_edit_ = -1;
  float encoder_ = 0.5f;
  float encoder_accumulator_ = 0.0f;
  int browse_index_ = 0;

  bool track_pad_held_[kNumTracks] = {};
  bool lower_pad_held_[8] = {};
  uint32_t seen_triggers_[kNumTracks] = {};
  double flash_time_[kNumTracks] = { -10, -10, -10, -10, -10, -10, -10, -10 };

  // Multitouch state.
  std::map<uint64_t, Finger> fingers_;
  bool mouse_finger_active_ = false;
  uint64_t mouse_finger_ = 0;
  std::vector<TouchTarget> touch_targets_;
  std::unordered_map<ImGuiID, KnobTouch> knob_touches_;
  std::unordered_map<ImGuiID, ButtonTouch> button_touches_;
  ImGuiID last_knob_tap_ = 0;
  double last_knob_tap_time_ = -10.0;
  bool track_pad_touched_[kNumTracks] = {};
  bool lower_pad_touched_[8] = {};
  bool xy_touched_ = false;
  float xy_touch_x_ = 0.5f;
  float xy_touch_y_ = 0.5f;
  bool long_press_fired_ = false;  // The held step already got its long press.
  // Erase held (button or Delete key): touching a knob then clears that
  // parameter's locks on the track, instead of the usual Erase on release.
  bool shift_held_ = false;
  bool step_held_by_finger_ = false;  // held_step_ lasts while the finger does.
  bool erase_held_ = false;
  bool erase_used_ = false;
  // Last Step and Copy held, and whether a page button was used meanwhile.
  bool last_step_held_ = false;
  bool last_step_used_ = false;
  bool copy_held_ = false;
  bool copy_used_ = false;
  int copy_source_page_ = -1;
  int copy_source_track_ = -1;
  // Erase + pads while playing: the pads held, and whether the gesture's
  // undo point was taken.
  bool erase_pad_held_[kNumTracks] = {};
  bool erase_wiping_ = false;
  // Mute and Solo held, and whether a pad was used meanwhile.
  bool mute_held_ = false;
  bool mute_used_ = false;
  bool solo_held_ = false;
  bool solo_used_ = false;
  // The steps follow the page playing (Shift + 15), until a page is chosen
  // by hand.
  bool follow_enabled_ = true;
  bool follow_playhead_ = true;
  bool was_playing_ = false;
  uint32_t seen_midi_activity_ = 0;
  double midi_flash_time_ = -10.0;

  std::vector<double> taps_;
  std::string toast_;
  double toast_time_ = -10.0;

  ClipboardKind clipboard_kind_ = CLIP_NONE;
  Step clip_step_;
  TrackSequence clip_track_;
  std::unique_ptr<Pattern> clip_pattern_;

  struct UndoEntry {
    int pattern;
    std::unique_ptr<Pattern> data;
  };
  std::deque<UndoEntry> undo_;
};

}  // namespace drumfreake

#endif  // DRUMFREAKE_UI_FACEPLATE_H_
