#include "core/project_io.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>

#include <nlohmann/json.hpp>

namespace drumfreake {

using nlohmann::json;

namespace {

const int kFormatVersion = 1;

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

json StepToJson(int index, const Step& s) {
  json j = {
    { "i", index },
    { "on", s.on },
    { "vel", s.velocity },
    { "prob", s.probability },
    { "rat", s.ratchets },
    { "note", s.note },
  };
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
  j["chords"] = std::vector<int>(project.chords, project.chords + 8);
  j["song"] = std::vector<int>(project.song, project.song + project.song_length);
  for (const TrackSound& sound : project.sounds) {
    j["sounds"].push_back({
      { "name", sound.name },
      { "params", FloatArray(sound.params) },
    });
  }
  for (int i = 0; i < kNumPatterns; ++i) {
    const Pattern& pattern = project.patterns[i];
    json jp = {
      { "length", pattern.length },
      { "swing", pattern.swing },
      { "poly", pattern.poly },
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
    if (j.contains("chords")) {
      for (size_t i = 0; i < 8 && i < j["chords"].size(); ++i) {
        loaded.chords[i] = static_cast<uint8_t>(j["chords"][i].get<int>());
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
      for (size_t i = 0; i < kNumTracks && i < j["sounds"].size(); ++i) {
        const json& js = j["sounds"][i];
        loaded.sounds[i].name = js.value("name", "Init");
        ReadFloatArray(js["params"], loaded.sounds[i].params);
      }
    }
    if (j.contains("patterns")) {
      for (size_t i = 0; i < kNumPatterns && i < j["patterns"].size(); ++i) {
        const json& jp = j["patterns"][i];
        Pattern& pattern = loaded.patterns[i];
        pattern.length = std::max(1, std::min(kMaxSteps, jp.value("length", 16)));
        pattern.swing = std::max(0.5f, std::min(0.75f, jp.value("swing", 0.5f)));
        pattern.poly = jp.value("poly", false);
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

}  // namespace drumfreake
