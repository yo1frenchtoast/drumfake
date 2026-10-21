// Audio engine: sequencer clock, 8 tracks, modulations and master section.
//
// Threading: the Project and every non-atomic member below are shared between
// the UI thread and the audio thread. The UI must hold lock() while it reads
// or writes them; Render() takes the same lock. The atomics (playhead, meters,
// scope) can be read from any thread without the lock.

#ifndef DRUMFAKE_AUDIO_ENGINE_H_
#define DRUMFAKE_AUDIO_ENGINE_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "audio/clouds_fx.h"
#include "audio/master_fx.h"
#include "audio/mod_sources.h"
#include "audio/track_voice.h"
#include "core/grids_gen.h"
#include "core/midi_parser.h"
#include "core/project.h"
#include "core/sample_file.h"

namespace drumfake {

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
  // Clouds freezes its recording buffer and loops it.
  bool clouds_freeze = false;

  // Song mode plays the project's chain of patterns in a loop.
  void SetSongMode(bool enabled);
  bool song_mode() const { return song_mode_; }
  // Index in the chain of the pattern playing (or about to play).
  int song_position() const { return song_position_; }

  int current_pattern() const { return current_pattern_; }
  int queued_pattern() const { return queued_pattern_; }
  // Kits: each pattern has its own, loaded with it. In Panel mode (like the
  // MatrixBrute's Panel button) the kit playing stays across pattern changes
  // and edits are stored nowhere until StoreKit(); leaving Panel mode reloads
  // the current pattern's kit.
  void SetPanel(bool panel);
  bool panel() const { return panel_; }
  // Snapshot (Shift + Panel): arming saves everything that can be played with (the sounds
  // playing, master, Clouds and Assign settings, tempo and volume, mute and solo, the Grids
  // position, the Keys and Chords settings) and behaves like Panel mode, so that no edit
  // reaches the patterns. Restoring puts it all back as it was and leaves Panel mode if it
  // was off before; the patterns' steps are never touched.
  void ArmSnapshot();
  void RestoreSnapshot();
  // Forgets the snapshot without restoring it (the project was replaced).
  void DropSnapshot() { snapshot_armed_ = false; }
  bool snapshot_armed() const { return snapshot_armed_; }
  // Writes the kit playing into the current pattern.
  void StoreKit();
  // Replaces the kit playing with the current pattern's (after the pattern
  // was loaded, pasted or restored). No effect in Panel mode.
  void ReloadKit();

  // Switches now when stopped, at the end of the pattern when playing.
  // Leaves song mode.
  void SelectPattern(int pattern);

  // Mute and solo: when any track is soloed, only soloed tracks play (the
  // sequence; pads always play).
  bool muted[kNumTracks] = {};
  bool soloed[kNumTracks] = {};
  bool TrackAudible(int track) const;
  int selected_track = 0;

  // Plays a track now (pads). When recording, also writes the step.
  void LiveTrigger(int track, float velocity, int note_offset);
  // Plays a chord on a track: `note_offset` and the `count` notes at `extras` semitones
  // above it. The extra notes use voices from a shared pool (kChordVoices), with the
  // sound and the effects of the track.
  void LiveChord(int track, float velocity, int note_offset, const int8_t* extras, int count);
  // When recording, writes a hit on the step nearest to the playhead.
  void RecordHit(int track, float velocity, int note_offset, int ratchets,
                 const int8_t* extras = nullptr, int count = 0);
  // For the tests: voices of the pool that are sounding.
  int chord_voices_active() const;

  // MIDI input: notes play tracks A-H; clock, Start, Stop, Continue and Song
  // Position drive the transport when the project follows the MIDI clock.
  // `time` (seconds, any monotonic clock) measures the external tempo.
  void HandleMidi(const MidiMessage& message, double time);
  // Tempo in use: the external clock's when following it.
  float tempo() const;
  // Prints every MIDI message received (--midi-monitor), except clock ticks.
  bool midi_monitor = false;

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

  // Sampler: the audio of each slot of the project's sample pool (null when
  // empty). SwapSample exchanges one slot with `*sample`, SwapSamples all of
  // them with `samples` (kNumSampleSlots); the previous ones come back there, to
  // be released once the lock is let go (never on the audio thread).
  void SwapSample(int slot, std::shared_ptr<const SampleData>* sample);
  void SwapSamples(std::shared_ptr<const SampleData>* samples);
  const SampleData* sample(int slot) const;

  // Vary: draws new variations for the current pattern (the "Dice").
  void DiceVary();

  // Writes one bar of the current Grids output into tracks A-C.
  // Loads `kit` (the 8 sounds) as the sounds playing; with `all_patterns`,
  // also as the kit of every pattern.
  void ApplyKit(const TrackSound* kit, bool all_patterns);
  void BakeGrids();
  // Writes a Marbles rhythm into the two tracks of project.marbles_tracks.
  void BakeMarbles(uint32_t seed);

  // --- Lock-free state for the UI. ------------------------------------------

  // Step currently playing on each track (-1 when stopped).
  std::atomic<int> track_step[kNumTracks];
  // Incremented each time a track fires (for the pad lights).
  std::atomic<uint32_t> trigger_count[kNumTracks];
  std::atomic<float> track_peak[kNumTracks];
  std::atomic<bool> track_gate[kNumTracks];  // Gate of the voice (for the tests).
  std::atomic<float> master_peak;
  std::atomic<int> pattern_step;  // Step within the pattern, -1 when stopped.
  // Master output, for the screen oscilloscope.
  std::atomic<float> scope[kScopeSize];
  std::atomic<int> scope_write;
  // Incremented by every MIDI message (activity LED), and the number of MIDI
  // inputs open (set by io/midi_input).
  std::atomic<uint32_t> midi_activity;
  std::atomic<int> midi_input_ports;
  // Seconds of audio rendered per second of wall time, for the CPU meter.
  std::atomic<float> cpu_load;

  // Parameters locked by the step that played the last note (bit per
  // TrackParam), and effective values after locks, LFOs and macros.
  std::atomic<uint32_t> active_lock_mask[kNumTracks];
  std::atomic<float> effective_param[kNumTracks][kNumLockable];

  // For tests: sample time and velocity of the last trigger on each track.
  std::atomic<int64_t> last_trigger_sample[kNumTracks];
  std::atomic<float> last_trigger_velocity[kNumTracks];

 private:
  void RenderBlock(float* out_l, float* out_r);
  void RunSequencer(double start, double end);
  void ProcessWindow(double start, double end, bool allow_pattern_change);
  void FireStep(int track, const Step& step, double step_duration);
  // The extra notes of a chord, each on a voice of the pool (the free one, or else the
  // one that started first).
  void FireChord(int track, float velocity, int note, const int8_t* extras, int count,
                 size_t max_gate, size_t gate_samples);
  // `gate_samples` overrides the Gate parameter; `slide_samples` glides the continuous
  // parameters from where they were to this note's locks over that time.
  void FireTrack(int track, float velocity, int note, const float* locks,
                 size_t max_gate, size_t gate_samples = 0, size_t slide_samples = 0);
  void ComputeParams(int track, float* params);
  float LfoValue(int track, int lfo) const;
  void RenderClick(float* out_l, float* out_r);
  int TrackStep(const Pattern& pattern, int track, int64_t n) const;
  bool RollProbability(int64_t step, int track, float probability) const;
  // Applies the pattern's Vary to step `s` of `track` (`stored`). Returns false
  // when the step does not play; otherwise `out` is the step to play.
  bool VaryStep(const Pattern& pattern, int track, int s, const Step& stored, Step* out) const;
  // Length of a step of the current pattern, in 16th notes (its division),
  // and durations in samples.
  double StepLength() const;
  size_t SamplesPerSixteenth() const;
  size_t SamplesPerStep() const;
  // Makes `pattern` current, swapping kits unless in Panel mode.
  void ChangePattern(int pattern);

  std::mutex mutex_;
  std::unique_ptr<Project> project_;
  std::shared_ptr<const SampleData> samples_[kNumSampleSlots];
  // What a track's voice plays: its sample when it is on the Sample engine.
  const SampleData* VoiceSample(const float* params) const;
  TrackVoice voices_[kNumTracks];
  static const int kChordVoices = 4;
  TrackVoice chord_voices_[kChordVoices];
  int chord_owner_[kChordVoices];       // Track whose sound a pool voice plays.
  int64_t chord_started_[kChordVoices];  // Sample time of its note.
  ModSources mods_[kNumTracks];
  std::unique_ptr<MasterFx> master_fx_;
  std::unique_ptr<CloudsFx> clouds_fx_;
  GridsGenerator grids_;

  bool playing_;
  bool song_mode_;
  bool panel_;
  struct Snapshot {
    TrackSound sounds[kNumTracks];
    float master[M_LAST];
    float assign[A_LAST];
    float clouds[C_LAST];
    float bpm;
    float volume;
    bool muted[kNumTracks];
    bool soloed[kNumTracks];
    bool grids_enabled;
    float grids_x;
    float grids_y;
    bool clouds_freeze;
    int key_root;
    int key_scale;
    int key_octave;
    int chord_type;
    bool panel;  // Panel mode before arming.
  };
  Snapshot snapshot_;
  bool snapshot_armed_ = false;
  int song_position_;
  int current_pattern_;
  int queued_pattern_;
  double position_;       // In steps since Play().
  double beat_position_;  // In 16th notes since Play() (metronome, LFOs...).
  double free_position_;  // Always running (LFOs and repeats when stopped).
  int64_t pattern_start_; // Step at which the current pattern started.
  int64_t poly_origin_;   // Step at which poly tracks last realigned.
  int64_t last_grids_step_;
  int64_t sample_counter_;

  // Parameter locks of the last note on each track (NAN = none).
  float active_locks_[kNumTracks][kNumLockable];
  // Slide: the value of each parameter after the base and the locks (before the
  // modulations) on the last block, where a slide starts from, and the slide itself.
  float lock_layer_[kNumTracks][kNumLockable];
  float slide_from_[kNumTracks][kNumLockable];
  size_t slide_total_[kNumTracks];
  size_t slide_left_[kNumTracks];
  // Step skipped once after a live-recorded note was rounded to it.
  int64_t skip_step_[kNumTracks];

  uint8_t repeat_mask_;
  int repeat_rate_;

  // Looper: loop start (in steps) and whether it is engaged.
  bool looping_;
  double loop_start_;
  double loop_length_;

  // External clock: ticks received since Start (-1 until the first one), and
  // the smoothed interval between ticks.
  int64_t clock_ticks_;
  double last_tick_time_;
  double tick_interval_;

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

}  // namespace drumfake

#endif  // DRUMFAKE_AUDIO_ENGINE_H_
