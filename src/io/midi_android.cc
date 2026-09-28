// Android MIDI input: DrumfreakeActivity.java opens the devices with the
// Java MIDI API and forwards their bytes here through JNI.

#include <jni.h>

#include <atomic>
#include <map>
#include <mutex>
#include <vector>

#include "io/midi_input.h"

namespace drumfreake {

namespace {

std::atomic<Engine*> g_engine{ nullptr };
std::atomic<int> g_ports{ 0 };
std::mutex g_parsers_mutex;
std::map<int, MidiParser> g_parsers;  // One per port (running status).

}  // namespace

struct MidiInput::Backend {
  Engine* engine;
};

MidiInput::MidiInput(Engine* engine) : backend_(new Backend()) {
  backend_->engine = engine;
}

MidiInput::~MidiInput() {
  g_engine.store(nullptr);
}

void MidiInput::Start() {
  g_engine.store(backend_->engine);
  Poll();
}

void MidiInput::Poll() {
  // Devices come and go on the Java side; just publish the count.
  backend_->engine->midi_input_ports.store(g_ports.load());
}

}  // namespace drumfreake

extern "C" JNIEXPORT void JNICALL
Java_org_drumfreake_app_DrumfreakeActivity_nativeMidi(
    JNIEnv* env, jclass, jint source, jbyteArray data, jint offset, jint count) {
  using namespace drumfreake;
  Engine* engine = g_engine.load();
  if (!engine || count <= 0) {
    return;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(count));
  env->GetByteArrayRegion(data, offset, count, reinterpret_cast<jbyte*>(bytes.data()));
  MidiParser* parser;
  {
    std::lock_guard<std::mutex> lock(g_parsers_mutex);
    parser = &g_parsers[source];  // std::map nodes don't move.
  }
  DeliverMidi(engine, parser, bytes.data(), bytes.size());
}

extern "C" JNIEXPORT void JNICALL
Java_org_drumfreake_app_DrumfreakeActivity_nativeMidiPorts(JNIEnv*, jclass, jint count) {
  drumfreake::g_ports.store(count);
}
