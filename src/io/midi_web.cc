// Web MIDI input: the browser opens every input with navigator.requestMIDIAccess
// and forwards their bytes here. Everything runs on the main thread.

#include <emscripten.h>

#include <map>

#include "io/midi_input.h"

namespace drumfake {

namespace {

Engine* g_engine = nullptr;
int g_ports = 0;
std::map<int, MidiParser> g_parsers;  // One per input (running status).

}  // namespace

}  // namespace drumfake

extern "C" {

EMSCRIPTEN_KEEPALIVE void drumfake_web_midi(int source, const uint8_t* data, int size) {
  using namespace drumfake;
  if (!g_engine || size <= 0) {
    return;
  }
  DeliverMidi(g_engine, &g_parsers[source], data, static_cast<size_t>(size));
}

EMSCRIPTEN_KEEPALIVE void drumfake_web_midi_ports(int count) {
  drumfake::g_ports = count;
}

}  // extern "C"

EM_JS_DEPS(web_midi, "$stackAlloc,$stackSave,$stackRestore");

// Asks for MIDI access (the browser may prompt), listens to every input and
// to the ones connected later.
EM_JS(void, StartWebMidi, (), {
  if (!navigator.requestMIDIAccess) {
    console.log("Drumfake: Web MIDI is not available in this browser");
    return;
  }
  navigator.requestMIDIAccess().then(function(access) {
    var sources = new Map();
    var update = function() {
      var count = 0;
      access.inputs.forEach(function(input) {
        if (input.state !== "connected") {
          return;
        }
        ++count;
        if (!sources.has(input.id)) {
          sources.set(input.id, sources.size);
        }
        var source = sources.get(input.id);
        input.onmidimessage = function(event) {
          var size = event.data.length;
          var top = stackSave();
          var pointer = stackAlloc(size);
          HEAPU8.set(event.data, pointer);
          _drumfake_web_midi(source, pointer, size);
          stackRestore(top);
        };
      });
      _drumfake_web_midi_ports(count);
    };
    access.onstatechange = update;
    update();
  }, function(error) {
    console.log("Drumfake: no MIDI access (" + error + ")");
  });
});

namespace drumfake {

struct MidiInput::Backend {
  Engine* engine;
};

MidiInput::MidiInput(Engine* engine) : backend_(new Backend()) {
  backend_->engine = engine;
}

MidiInput::~MidiInput() {
  g_engine = nullptr;
}

void MidiInput::Start() {
  g_engine = backend_->engine;
  StartWebMidi();
  Poll();
}

void MidiInput::Poll() {
  // Inputs come and go on the JavaScript side; just publish the count.
  backend_->engine->midi_input_ports.store(g_ports);
}

}  // namespace drumfake
