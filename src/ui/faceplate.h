// The Drumfake front panel: layout, controls, screen and keyboard shortcuts.

#ifndef DRUMFAKE_UI_FACEPLATE_H_
#define DRUMFAKE_UI_FACEPLATE_H_

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/engine.h"
#include "core/project_io.h"
#include "ui/widgets.h"

namespace drumfake {

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
  // A file dropped on the window at `position` (display coordinates): a WAV is
  // imported onto the pad under it, or onto the selected track. Takes the lock.
  void DropFile(const std::string& path, ImVec2 position);

 private:
  enum PadMode { PADS_SOUNDS, PADS_REPEATS, PADS_CHORDS, PADS_KEYS, PADS_SLICES };
  enum ScreenView { VIEW_MAIN, VIEW_EDIT, VIEW_BROWSE, VIEW_MIDI, VIEW_MARBLES, VIEW_KITS,
                    VIEW_SLICES };
  enum ClipboardKind { CLIP_NONE, CLIP_STEP, CLIP_TRACK, CLIP_PATTERN };

  void DrawChrome();
  void DrawTopLeft();
  void DrawXySection();
  void DrawModMatrix(ImVec2 min, ImVec2 max);
  void DrawVaryPad(ImVec2 min, ImVec2 max);
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
  // Name and value bubble above a knob being touched or hovered (works for fingers,
  // unlike ImGui tooltips). Drawn on the foreground list so nothing covers it.
  void DrawKnobReadout(ImVec2 center, float radius, const char* name, const std::string& value);
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
  // The black key on instrument pad `pad` (A-H) in Keys mode, as an offset from the
  // track pitch; false when this pad has none.
  bool KeySharp(int pad, int* offset);
  // Plays a key of the Keys mode on the selected track, and writes it into the held step.
  void PlayKey(int offset);
  void PlayChord(int pad);
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

  Engine* engine_ = nullptr;
  Canvas canvas_;
  std::string project_path_;
  // File work queued while drawing (under the engine lock) and done after it,
  // since loading samples takes the lock itself.
  struct PendingImport {
    std::string path;
    int track;
  };
  std::vector<PendingImport> pending_imports_;
  bool reload_samples_ = false;
  void DoFileWork();
  void DrawPanel();

  // Sampler.
  // Name, short name and value shown for knob `k` of the current page: on the
  // Sound page, the Sample engine gives Harmonics, Timbre, Morph, Decay and Aux
  // their own meaning.
  void KnobLabel(int k, float value, const char** short_name, const char** name,
                 std::string* text) const;
  bool OnSampleEngine() const;
  // Puts the selected track on `slot` (Browse > Samples).
  void PlaceSample(int track, int slot);
  void DrawSampleBrowser(ImVec2 min, ImVec2 max);
  // The sample of the selected track, with Start, end of Length and Loop.
  void DrawSampleOverview(ImVec2 min, ImVec2 max);
  bool browse_samples_ = false;  // Browse shows the sample pool.
  float auto_sensitivity_ = 0.5f;  // Share of the attacks Auto slicing keeps.
  int auto_sliced_slot_ = -1;      // Sliced by Auto: the sensitivity slider redoes it.
  // Slice editor (Browse > Samples > Edit): the slot edited, the selected slice,
  // the zoom (1 = the whole sample) around a centre, the marker being dragged.
  void DrawSliceEditor(ImVec2 min, ImVec2 max);
  // Moves the selected slice's start by `delta` (fraction of the sample),
  // snapped to a zero crossing.
  void NudgeSlice(float delta);
  void PreviewSlice(int slice);
  int slice_edit_slot_ = -1;
  int selected_slice_ = 0;
  float slice_zoom_ = 1.0f;
  float slice_center_ = 0.5f;
  int dragging_slice_ = -1;
  // Peaks of 64-frame blocks of the edited sample, for drawing it zoomed.
  const SampleData* slice_peaks_sample_ = nullptr;
  size_t slice_peaks_frames_ = 0;
  std::vector<float> slice_peaks_;
  int browse_sample_page_ = 0;   // 16 slots per page.
  // Overview of the last sample drawn: peak per column, rebuilt when it changes.
  const SampleData* overview_sample_ = nullptr;
  size_t overview_frames_ = 0;
  std::vector<float> overview_;

  Page page_ = PAGE_SOUND;
  PadMode pad_mode_ = PADS_SOUNDS;
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
  int slice_bank_ = 0;  // Slices mode: pads 1-8 play slices 8 * bank + 1 to 8.
  // The slices of the selected track's sample, or null (not on the Sample engine).
  const SampleSlices* TrackSlices() const;
  void PlaySlice(int pad);
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
  bool panel_long_fired_ = false;  // The finger on Panel has armed (or restored) the snapshot.
  bool matrix_open_ = false;  // The Perform pad shows the modulation matrix.
  bool vary_open_ = false;    // The Perform pad sets the pattern's Vary (Shift + Grids).
  int matrix_page_ = 0;       // Destinations shown: Sound, Filter or Track params.
  float xy_touch_x_ = 0.5f;
  float xy_touch_y_ = 0.5f;
  // Hold: the FX, Looper or Macros pad keeps acting at this position after it is
  // released (lit mode button pressed while touching the pad, or right click on it).
  bool xy_held_ = false;
  float xy_hold_x_ = 0.5f;
  float xy_hold_y_ = 0.5f;
  bool long_press_fired_ = false;  // The held step already got its long press.
  // Erase held (button or Delete key): touching a knob then clears that
  // parameter's locks on the track, instead of the usual Erase on release.
  bool shift_held_ = false;
  bool shift_latched_ = false;  // Shift locked by a right click, until a combination.
  bool shift_used_ = false;     // A Shift combination was used this frame.
  bool step_held_by_finger_ = false;  // held_step_ lasts while the finger does.
  bool erase_held_ = false;
  bool erase_used_ = false;
  // Last Step and Copy held, and whether a page button was used meanwhile.
  bool last_step_held_ = false;
  bool last_step_used_ = false;
  bool last_step_latched_ = false;  // Last Step locked by a right click.
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

}  // namespace drumfake

#endif  // DRUMFAKE_UI_FACEPLATE_H_
