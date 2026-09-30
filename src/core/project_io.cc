#include "core/project_io.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

#include <nlohmann/json.hpp>

#include "core/scales.h"

namespace drumfreake {

using nlohmann::json;

namespace {

const int kFormatVersion = 2;  // 2: one kit per pattern.

template<size_t N>
json FloatArray(const float (&values)[N]) {
  return json(std::vector<float>(values, values + N));
}

template<size_t N>
void ReadFloatArray(const json& j, float (&values)[N]) {
  if (!j.is_array()) {
    return;
  }
  for (size_t i = 0; i < N && i < j.size(); ++i) {
    values[i] = Clamp01(j[i].get<float>());
  }
}

json KitToJson(const TrackSound* kit) {
  json j = json::array();
  for (int i = 0; i < kNumTracks; ++i) {
    j.push_back({ { "name", kit[i].name }, { "params", FloatArray(kit[i].params) } });
  }
  return j;
}

void KitFromJson(const json& j, TrackSound* kit) {
  for (size_t i = 0; i < kNumTracks && i < j.size(); ++i) {
    kit[i].Reset();
    kit[i].name = j[i].value("name", "Init");
    ReadFloatArray(j[i]["params"], kit[i].params);
  }
}

json StepToJson(int index, const Step& s) {
  json j = {
    { "i", index },
    { "on", s.on },
    { "vel", s.velocity },
    { "prob", s.probability },
    { "rat", s.ratchets },
    { "note", s.note },
  };
  if (s.micro != 0.0f) {
    j["micro"] = s.micro;
  }
  json locks = json::object();
  for (int p = 0; p < kNumLockable; ++p) {
    if (s.has_lock(p)) {
      locks[std::to_string(p)] = s.locks[p];
    }
  }
  if (!locks.empty()) {
    j["locks"] = locks;
  }
  return j;
}

void StepFromJson(const json& j, Step* s) {
  s->Clear();
  s->on = j.value("on", false);
  s->velocity = Clamp01(j.value("vel", 0.8f));
  s->probability = Clamp01(j.value("prob", 1.0f));
  s->ratchets = std::max(1, std::min(kMaxRatchets, j.value("rat", 1)));
  s->note = std::max(-48, std::min(48, j.value("note", 0)));
  s->micro = std::max(-kMaxMicroTiming, std::min(kMaxMicroTiming, j.value("micro", 0.0f)));
  if (j.contains("locks")) {
    for (auto& [key, value] : j["locks"].items()) {
      int p = std::atoi(key.c_str());
      if (p >= 0 && p < kNumLockable) {
        s->locks[p] = Clamp01(value.get<float>());
      }
    }
  }
}

}  // namespace

bool SaveProject(const Project& project, const std::string& path, std::string* error) {
  json j;
  j["version"] = kFormatVersion;
  j["bpm"] = project.bpm;
  j["volume"] = project.volume;
  j["master"] = FloatArray(project.master);
  j["assign"] = FloatArray(project.assign);
  j["clouds"] = FloatArray(project.clouds);
  j["chords"] = std::vector<int>(project.chords, project.chords + 8);
  j["switch_at_end"] = project.switch_at_end;
  j["midi"] = {
    { "clock_sync", project.midi_clock_sync },
    { "channels", std::vector<int>(project.midi_channels, project.midi_channels + kNumTracks) },
    { "trigger_note", project.midi_trigger_note },
  };
  j["keys"] = {
    { "root", project.key_root },
    { "scale", project.key_scale },
    { "octave", project.key_octave },
  };
  j["marbles"] = {
    { "model", project.marbles.model },
    { "bias", project.marbles.bias },
    { "deja_vu", project.marbles.deja_vu },
    { "length", project.marbles.length },
    { "tracks", std::vector<int>(project.marbles_tracks, project.marbles_tracks + 2) },
  };
  j["song"] = std::vector<int>(project.song, project.song + project.song_length);
  j["sounds"] = KitToJson(project.sounds);
  for (int i = 0; i < kNumPatterns; ++i) {
    const Pattern& pattern = project.patterns[i];
    json jp = {
      { "length", pattern.length },
      { "swing", pattern.swing },
      { "poly", pattern.poly },
      { "division", pattern.division },
      { "kit", KitToJson(pattern.kit) },
    };
    for (const TrackSequence& track : pattern.tracks) {
      json jt = { { "length", track.length }, { "steps", json::array() } };
      for (int s = 0; s < kMaxSteps; ++s) {
        if (track.steps[s].on || track.steps[s].has_any_lock()) {
          jt["steps"].push_back(StepToJson(s, track.steps[s]));
        }
      }
      jp["tracks"].push_back(jt);
    }
    j["patterns"].push_back(jp);
  }

  std::error_code ec;
  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::filesystem::create_directories(p.parent_path(), ec);
  }
  std::ofstream out(path);
  if (!out) {
    *error = "cannot write " + path;
    return false;
  }
  out << j.dump(1);
  return static_cast<bool>(out);
}

bool LoadProject(const std::string& path, Project* project, std::string* error) {
  std::ifstream in(path);
  if (!in) {
    *error = "cannot read " + path;
    return false;
  }
  json j;
  try {
    in >> j;
    auto loaded_ptr = std::make_unique<Project>();
    Project& loaded = *loaded_ptr;
    loaded.Init();
    for (Pattern& pattern : loaded.patterns) {
      pattern.Clear();
    }
    loaded.bpm = std::max(30.0f, std::min(300.0f, j.value("bpm", 120.0f)));
    loaded.volume = Clamp01(j.value("volume", 0.8f));
    ReadFloatArray(j["master"], loaded.master);
    ReadFloatArray(j["assign"], loaded.assign);
    ReadFloatArray(j["clouds"], loaded.clouds);
    if (j.contains("chords")) {
      for (size_t i = 0; i < 8 && i < j["chords"].size(); ++i) {
        loaded.chords[i] = static_cast<uint8_t>(j["chords"][i].get<int>());
      }
    }
    loaded.switch_at_end = j.value("switch_at_end", true);
    if (j.contains("midi")) {
      const json& midi = j["midi"];
      loaded.midi_clock_sync = midi.value("clock_sync", false);
      if (midi.contains("channels")) {
        for (size_t i = 0; i < kNumTracks && i < midi["channels"].size(); ++i) {
          loaded.midi_channels[i] = std::max(0, std::min(16, midi["channels"][i].get<int>()));
        }
      }
      loaded.midi_trigger_note = std::max(0, std::min(115, midi.value("trigger_note", 36)));
    }
    if (j.contains("keys")) {
      const json& keys = j["keys"];
      loaded.key_root = std::max(0, std::min(11, keys.value("root", 0)));
      loaded.key_scale = std::max(0, std::min(kNumScales - 1, keys.value("scale", 0)));
      loaded.key_octave = std::max(kMinKeyOctave, std::min(kMaxKeyOctave, keys.value("octave", 0)));
    }
    if (j.contains("marbles")) {
      const json& m = j["marbles"];
      MarblesSettings& s = loaded.marbles;
      s.model = std::max(0, std::min(kNumMarblesModels - 1, m.value("model", 0)));
      s.bias = Clamp01(m.value("bias", 0.5f));
      s.deja_vu = Clamp01(m.value("deja_vu", 0.5f));
      s.length = std::max(1, std::min(kMaxDejaVuLength, m.value("length", 8)));
      if (m.contains("tracks")) {
        for (size_t i = 0; i < 2 && i < m["tracks"].size(); ++i) {
          loaded.marbles_tracks[i] = std::max(0, std::min(kNumTracks - 1, m["tracks"][i].get<int>()));
        }
      }
    }
    if (j.contains("song")) {
      for (const json& entry : j["song"]) {
        int pattern = entry.get<int>();
        if (loaded.song_length < kMaxSongLength && pattern >= 0 && pattern < kNumPatterns) {
          loaded.song[loaded.song_length++] = pattern;
        }
      }
    }
    if (j.contains("sounds")) {
      KitFromJson(j["sounds"], loaded.sounds);
    }
    // Projects saved before kits were per pattern share one kit.
    for (Pattern& pattern : loaded.patterns) {
      std::copy(loaded.sounds, loaded.sounds + kNumTracks, pattern.kit);
    }
    if (j.contains("patterns")) {
      for (size_t i = 0; i < kNumPatterns && i < j["patterns"].size(); ++i) {
        const json& jp = j["patterns"][i];
        Pattern& pattern = loaded.patterns[i];
        pattern.length = std::max(1, std::min(kMaxSteps, jp.value("length", 16)));
        pattern.swing = std::max(0.5f, std::min(0.75f, jp.value("swing", 0.5f)));
        pattern.poly = jp.value("poly", false);
        pattern.division = std::max(0, std::min(kNumDivisions - 1,
                                                jp.value("division", kDefaultDivision)));
        if (jp.contains("kit")) {
          KitFromJson(jp["kit"], pattern.kit);
        }
        if (!jp.contains("tracks")) {
          continue;
        }
        for (size_t t = 0; t < kNumTracks && t < jp["tracks"].size(); ++t) {
          const json& jt = jp["tracks"][t];
          TrackSequence& track = pattern.tracks[t];
          track.length = std::max(0, std::min(kMaxSteps, jt.value("length", 0)));
          for (const json& js : jt["steps"]) {
            int s = js.value("i", -1);
            if (s >= 0 && s < kMaxSteps) {
              StepFromJson(js, &track.steps[s]);
            }
          }
        }
      }
    }
    *project = loaded;
  } catch (const std::exception& e) {
    *error = std::string("invalid project file: ") + e.what();
    return false;
  }
  return true;
}

std::string DefaultProjectPath() {
  const char* xdg = std::getenv("XDG_DATA_HOME");
  std::string base;
  if (xdg && *xdg) {
    base = xdg;
  } else {
    const char* home = std::getenv("HOME");
    base = std::string(home ? home : ".") + "/.local/share";
  }
  return base + "/drumfreake/project.json";
}

std::string KitDirectory(const std::string& project_path) {
  return (std::filesystem::path(project_path).parent_path() / "kits").string();
}

namespace {

// Number N of "kit_NNN.json", or -1.
int KitNumber(const std::filesystem::path& path) {
  std::string stem = path.stem().string();
  if (path.extension() != ".json" || stem.rfind("kit_", 0) != 0) {
    return -1;
  }
  char* end = nullptr;
  long n = std::strtol(stem.c_str() + 4, &end, 10);
  return (end && *end == 0 && n >= 0) ? static_cast<int>(n) : -1;
}

}  // namespace

std::vector<KitFile> ListKits(const std::string& directory) {
  std::vector<std::pair<int, KitFile>> found;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
    int number = KitNumber(entry.path());
    if (number < 0) {
      continue;
    }
    KitFile kit;
    kit.path = entry.path().string();
    kit.name = "Kit " + std::to_string(number);
    try {
      std::ifstream file(kit.path);
      json j = json::parse(file);
      const json& tracks = j.at("tracks");
      for (size_t i = 0; i < 4 && i < tracks.size(); ++i) {
        kit.summary += std::string(i ? ", " : "") + tracks[i].value("name", "Init");
      }
    } catch (const std::exception&) {
      continue;  // Not a kit file.
    }
    found.emplace_back(number, kit);
  }
  std::sort(found.begin(), found.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  std::vector<KitFile> kits;
  for (auto& entry : found) {
    kits.push_back(entry.second);
  }
  return kits;
}

bool SaveKitFile(const std::string& directory, const TrackSound* kit,
                 const std::string& path, std::string* error) {
  std::string target = path;
  std::error_code ec;
  if (target.empty()) {
    int next = 1;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
      next = std::max(next, KitNumber(entry.path()) + 1);
    }
    char name[32];
    snprintf(name, sizeof(name), "kit_%03d.json", next);
    target = (std::filesystem::path(directory) / name).string();
  }
  std::filesystem::create_directories(directory, ec);
  std::ofstream file(target);
  if (!file) {
    *error = "cannot write " + target;
    return false;
  }
  json j;
  j["version"] = 1;
  j["tracks"] = KitToJson(kit);
  file << j.dump(1);
  return true;
}

bool LoadKitFile(const std::string& path, TrackSound* kit, std::string* error) {
  try {
    std::ifstream file(path);
    json j = json::parse(file);
    KitFromJson(j.at("tracks"), kit);
    return true;
  } catch (const std::exception& e) {
    *error = e.what();
    return false;
  }
}

bool DeleteKitFile(const std::string& path, std::string* error) {
  std::error_code ec;
  if (!std::filesystem::remove(path, ec) || ec) {
    *error = "cannot delete " + path;
    return false;
  }
  return true;
}

}  // namespace drumfreake
