// Turns a MIDI byte stream into messages: running status, real-time bytes
// interleaved with other messages, System Exclusive skipped.

#ifndef DRUMFREAKE_CORE_MIDI_PARSER_H_
#define DRUMFREAKE_CORE_MIDI_PARSER_H_

#include <cstddef>
#include <cstdint>

namespace drumfreake {

struct MidiMessage {
  uint8_t status;  // With the channel in the low nibble for channel messages.
  uint8_t data[2];
  int size;        // Number of bytes, status included.

  int channel() const { return status & 0x0f; }  // 0-15.
  uint8_t type() const { return status < 0xf0 ? status & 0xf0 : status; }
};

enum MidiStatus : uint8_t {
  MIDI_NOTE_OFF = 0x80,
  MIDI_NOTE_ON = 0x90,
  MIDI_SONG_POSITION = 0xf2,
  MIDI_CLOCK = 0xf8,
  MIDI_START = 0xfa,
  MIDI_CONTINUE = 0xfb,
  MIDI_STOP = 0xfc,
};

class MidiParser {
 public:
  // Calls handler(const MidiMessage&) for each complete message in `bytes`.
  template<typename Handler>
  void Parse(const uint8_t* bytes, size_t size, Handler handler) {
    MidiMessage message;
    for (size_t i = 0; i < size; ++i) {
      if (Feed(bytes[i], &message)) {
        handler(message);
      }
    }
  }

  // Returns true when `byte` completes a message.
  bool Feed(uint8_t byte, MidiMessage* message);

 private:
  static int DataBytes(uint8_t status);

  uint8_t running_status_ = 0;
  uint8_t data_[2] = {};
  int received_ = 0;
  bool in_sysex_ = false;
};

}  // namespace drumfreake

#endif  // DRUMFREAKE_CORE_MIDI_PARSER_H_
