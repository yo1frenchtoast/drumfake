// MIDI input from the platform (RtMidi on the desktop, the Android MIDI API
// on Android), delivered to the engine.

#ifndef DRUMFREAKE_IO_MIDI_INPUT_H_
#define DRUMFREAKE_IO_MIDI_INPUT_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "audio/engine.h"
#include "core/midi_parser.h"

namespace drumfreake {

class MidiInput {
 public:
  explicit MidiInput(Engine* engine);
  ~MidiInput();

  // Opens every input (and, on the desktop, a virtual "Drumf(re)ake" port).
  void Start();
  // Opens inputs connected since the last call. Call it every few seconds.
  void Poll();

 private:
  struct Backend;
  std::unique_ptr<Backend> backend_;
};

// Parses bytes from one source and hands the messages to the engine, under
// its lock. `parser` keeps the source's running status.
void DeliverMidi(Engine* engine, MidiParser* parser, const uint8_t* bytes, size_t size);

}  // namespace drumfreake

#endif  // DRUMFREAKE_IO_MIDI_INPUT_H_
