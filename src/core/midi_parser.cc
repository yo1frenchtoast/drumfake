#include "core/midi_parser.h"

namespace drumfake {

int MidiParser::DataBytes(uint8_t status) {
  switch (status & 0xf0) {
    case 0xc0:  // Program change.
    case 0xd0:  // Channel pressure.
      return 1;
    case 0xf0:
      switch (status) {
        case 0xf1:  // Time code quarter frame.
        case 0xf3:  // Song select.
          return 1;
        case MIDI_SONG_POSITION:
          return 2;
        default:
          return 0;
      }
    default:
      return 2;
  }
}

bool MidiParser::Feed(uint8_t byte, MidiMessage* message) {
  if (byte >= 0xf8) {
    // Real-time messages can appear anywhere, even inside other messages.
    message->status = byte;
    message->size = 1;
    return true;
  }
  if (byte == 0xf0) {
    in_sysex_ = true;
    running_status_ = 0;
    return false;
  }
  if (byte >= 0x80) {
    in_sysex_ = false;
    received_ = 0;
    // System common messages cancel the running status.
    running_status_ = byte < 0xf0 ? byte : 0;
    if (byte >= 0xf0) {
      if (DataBytes(byte) == 0) {
        if (byte == 0xf7) {
          return false;  // End of exclusive.
        }
        message->status = byte;
        message->size = 1;
        return true;
      }
      running_status_ = byte;  // Until its data bytes arrive.
    }
    return false;
  }
  // Data byte.
  if (in_sysex_ || !running_status_) {
    return false;
  }
  data_[received_++] = byte;
  if (received_ < DataBytes(running_status_)) {
    return false;
  }
  message->status = running_status_;
  message->data[0] = data_[0];
  message->data[1] = received_ > 1 ? data_[1] : 0;
  message->size = 1 + received_;
  received_ = 0;
  if (running_status_ >= 0xf0) {
    running_status_ = 0;  // No running status for system common messages.
  }
  return true;
}

}  // namespace drumfake
