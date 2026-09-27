// Audio engine: sequencer clock, 8 tracks, modulations and master section.
//
// Threading: the Project and every non-atomic member below are shared between
// the UI thread and the audio thread. The UI must hold lock() while it reads
// or writes them; Render() takes the same lock. The atomics (playhead, meters,
// scope) can be read from any thread without the lock.

#ifndef DRUMFREAK_AUDIO_ENGINE_H_
#define DRUMFREAK_AUDIO_ENGINE_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "audio/master_fx.h"
#include "audio/track_voice.h"
#include "core/grids_gen.h"
#include "core/project.h"

namespace drumfreak {

// What the XY pad does (the four buttons above it).
enum XyMode {
  XY_GRIDS,   // "Spice": position in the Grids drum map.
  XY_FX,      // X = DJ filter, Y = delay throw.
  XY_LOOPER,  // Y = loop length, while touched.
  XY_MACROS,  // X/Y = assigned parameters of the selected track.
};

static_assert(kNumLockable <= 32, "active_lock_mask holds one bit per parameter");

const int kNumRepeatRates = 8;
const int kScopeSize = 1024;

class Engine {
 public:
  Engine();
  ~Engine();

  void Init();
  std::mutex& mutex() { return mutex_; }

  // Renders interleaved stereo float frames. Takes the lock.
  void Render(float* out, size_t frames);

  // --- Everything below requires the lock. ---------------------------------

  Project& project() { return *project_; }

  void Play();
  void Stop();
  bool playing() const { return playing_; }
  bool recording = false;
  bool metronome = false;

  // Song mode plays the project's chain of patterns in a loop.
  void SetSongMode(bool enabled);
  bool song_mode() const { return song_mode_; }
  // Index in the chain of the pattern playing (or about to play).
  int song_position() const { return song_position_; }

  int current_pattern() const { return current_pattern_; }
  int queued_pattern() const { return queued_pattern_; }
  // Switches now when stopped, at the end of the pattern when playing.
  // Leaves song mode, like selecting a pattern on the Arturia machines.
  void SelectPattern(int pattern);

  bool muted[kNumTracks] = {};
  int selected_track = 0;

  // Plays a track now (pads). When recording, also writes the step.
  void LiveTrigger(int track, float velocity, int note_offset);

  // Note repeat: tracks in `mask` retrigger at `rate` (0 to 7) while held.
  void SetRepeat(uint8_t mask, int rate);

  XyMode xy_mode = XY_GRIDS;
  bool grids_enabled = false;
  float grids_x = 0.5f;
  float grids_y = 0.5f;
  // Pad state for the momentary modes (FX, looper, macros).
  bool xy_touched = false;
  float xy_x = 0.5f;
  float xy_y = 0.5f;

  // Writes one bar of the current Grids output into tracks A-C.
  void BakeGrids();

  // --- Lock-free state for the UI. ------------------------------------------

  // Step currently playing on each track (-1 when stopped).
  std::atomic<int> track_step[kNumTracks];
  // Incremented each time a track fires (for the pad lights).
  std::atomic<uint32_t> trigger_count[kNumTracks];
  std::atomic<float> track_peak[kNumTracks];
  std::atomic<float> master_peak;
  std::atomic<int> pattern_step;  // Step within the pattern, -1 when stopped.
  // Master output, for the screen oscilloscope.
  std::atomic<float> scope[kScopeSize];
  std::atomic<int> scope_write;
  // Seconds of audio rendered per second of wall time, for the CPU meter.
  std::atomic<float> cpu_load;

  // Parameters locked by the step that played the last note (bit per
  // TrackParam), and effective values after locks, LFOs and macros.
  std::atomic<uint32_t> active_lock_mask[kNumTracks];
  std::atomic<float> effective_param[kNumTracks][kNumLockable];

  // For tests: sample time of the last trigger on each track.
  std::atomic<int64_t> last_trigger_sample[kNumTracks];

 private:
  void RenderBlock(float* out_l, float* out_r);
  void RunSequencer(double start, double end);
  void ProcessWindow(double start, double end, bool allow_pattern_change);
  void FireStep(int track, const Step& step, double step_duration);
  void FireTrack(int track, float velocity, int note, const float* locks,
                 size_t max_gate);
  void ComputeParams(int track, float* params);
  float LfoValue(int track, int lfo) const;
  void RenderClick(float* out_l, float* out_r);
  int TrackStep(const Pattern& pattern, int track, int64_t n) const;
  bool RollProbability(int64_t step, int track, float probability) const;
  size_t SamplesPerStep() const;

  std::mutex mutex_;
  std::unique_ptr<Project> project_;
  TrackVoice voices_[kNumTracks];
  std::unique_ptr<MasterFx> master_fx_;
  GridsGenerator grids_;

  bool playing_;
  bool song_mode_;
  int song_position_;
  int current_pattern_;
  int queued_pattern_;
  double position_;       // In 16th notes since Play().
  double free_position_;  // Always running (LFOs and repeats when stopped).
  int64_t pattern_start_; // Step at which the current pattern started.
  int64_t poly_origin_;   // Step at which poly tracks last realigned.
  int64_t last_grids_step_;
  int64_t sample_counter_;

  // Parameter locks of the last note on each track (NAN = none).
  float active_locks_[kNumTracks][kNumLockable];
  // Step skipped once after a live-recorded note was rounded to it.
  int64_t skip_step_[kNumTracks];

  uint8_t repeat_mask_;
  int repeat_rate_;

  // Looper: loop start (in steps) and whether it is engaged.
  bool looping_;
  double loop_start_;
  double loop_length_;

  float click_phase_;
  float click_envelope_;
  float click_frequency_;

  float block_l_[kBlockSize];
  float block_r_[kBlockSize];
  size_t block_read_;
  Buses buses_;
};

// Repeat periods in 16th notes, for pads 1-8 in "Repeats" mode.
extern const float kRepeatPeriods[kNumRepeatRates];
extern const char* const kRepeatNames[kNumRepeatRates];

}  // namespace drumfreak

#endif  // DRUMFREAK_AUDIO_ENGINE_H_
