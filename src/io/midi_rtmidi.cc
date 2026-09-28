// Desktop MIDI input with RtMidi: every hardware/software input port, plus a
// virtual port other applications can connect to.

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "RtMidi.h"
#include "io/midi_input.h"

namespace drumfreake {

namespace {

const char* const kClientName = "Drumf(re)ake";

struct Source {
  Engine* engine;
  MidiParser parser;
  std::unique_ptr<RtMidiIn> input;
};

void Callback(double, std::vector<unsigned char>* message, void* user_data) {
  Source* source = static_cast<Source*>(user_data);
  DeliverMidi(source->engine, &source->parser, message->data(), message->size());
}

std::unique_ptr<RtMidiIn> NewInput() {
  auto input = std::make_unique<RtMidiIn>(RtMidi::UNSPECIFIED, kClientName);
  // Keep clock messages; drop SysEx and active sensing.
  input->ignoreTypes(true, false, true);
  return input;
}

}  // namespace

struct MidiInput::Backend {
  Engine* engine;
  std::unique_ptr<RtMidiIn> probe;
  Source virtual_port;
  std::map<std::string, std::unique_ptr<Source>> ports;
};

MidiInput::MidiInput(Engine* engine) : backend_(new Backend()) {
  backend_->engine = engine;
}

MidiInput::~MidiInput() {
  if (!backend_) {
    return;
  }
  for (auto& [name, source] : backend_->ports) {
    source->input->closePort();
  }
  if (backend_->virtual_port.input) {
    backend_->virtual_port.input->closePort();
  }
}

void MidiInput::Start() {
  try {
    backend_->probe = NewInput();
    Source& v = backend_->virtual_port;
    v.engine = backend_->engine;
    v.input = NewInput();
    v.input->setCallback(&Callback, &v);
    v.input->openVirtualPort(kClientName);
  } catch (const RtMidiError& e) {
    fprintf(stderr, "MIDI: %s\n", e.getMessage().c_str());
    backend_->virtual_port.input.reset();
  }
  Poll();
}

void MidiInput::Poll() {
  if (!backend_->probe) {
    return;
  }
  try {
    std::vector<std::string> names;
    unsigned int count = backend_->probe->getPortCount();
    for (unsigned int i = 0; i < count; ++i) {
      names.push_back(backend_->probe->getPortName(i));
    }
    // Forget ports that disappeared.
    for (auto it = backend_->ports.begin(); it != backend_->ports.end();) {
      bool present = false;
      for (const std::string& name : names) {
        present = present || name == it->first;
      }
      it = present ? std::next(it) : backend_->ports.erase(it);
    }
    for (unsigned int i = 0; i < names.size(); ++i) {
      const std::string& name = names[i];
      if (name.find(kClientName) != std::string::npos || backend_->ports.count(name)) {
        continue;  // Our own virtual port, or already open.
      }
      auto source = std::make_unique<Source>();
      source->engine = backend_->engine;
      source->input = NewInput();
      source->input->setCallback(&Callback, source.get());
      source->input->openPort(i, kClientName);
      fprintf(stderr, "MIDI: listening to %s\n", name.c_str());
      backend_->ports[name] = std::move(source);
    }
    backend_->engine->midi_input_ports.store(static_cast<int>(backend_->ports.size()));
  } catch (const RtMidiError& e) {
    fprintf(stderr, "MIDI: %s\n", e.getMessage().c_str());
  }
}

}  // namespace drumfreake
