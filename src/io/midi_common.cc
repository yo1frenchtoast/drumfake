#include <chrono>
#include <mutex>

#include "io/midi_input.h"

namespace drumfreake {

void DeliverMidi(Engine* engine, MidiParser* parser, const uint8_t* bytes, size_t size) {
  double time = std::chrono::duration<double>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  std::lock_guard<std::mutex> lock(engine->mutex());
  parser->Parse(bytes, size, [&](const MidiMessage& message) {
    engine->HandleMidi(message, time);
  });
}

}  // namespace drumfreake
