cmake_minimum_required(VERSION 3.20)
project(drumfake CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(DRUMFAKE_GUI "Build the SDL3/ImGui front-end" ON)

if(EMSCRIPTEN)
  # nlohmann-json reports parse errors with exceptions: catch them in wasm too.
  add_compile_options(-fexceptions)
  add_link_options(-fexceptions)
endif()

# Homebrew (linuxbrew) provides SDL3, OpenGL and nlohmann-json on this system.
if(EXISTS /home/linuxbrew/.linuxbrew)
  list(APPEND CMAKE_PREFIX_PATH /home/linuxbrew/.linuxbrew)
endif()

set(EURORACK_DIR ${CMAKE_SOURCE_DIR}/third_party/eurorack)
if(NOT EXISTS ${EURORACK_DIR}/stmlib/stmlib.h)
  message(FATAL_ERROR "Submodules missing: run 'git submodule update --init --recursive'")
endif()

include(FetchContent)

# --- Mutable Instruments DSP (unmodified, from the submodule) ----------------

file(GLOB PLAITS_DSP_SOURCES CONFIGURE_DEPENDS
  ${EURORACK_DIR}/plaits/dsp/*.cc
  ${EURORACK_DIR}/plaits/dsp/*/*.cc)

add_library(mutable_dsp STATIC
  ${PLAITS_DSP_SOURCES}
  ${EURORACK_DIR}/plaits/resources.cc
  ${EURORACK_DIR}/rings/resources.cc
  ${EURORACK_DIR}/grids/resources.cc
  ${EURORACK_DIR}/peaks/resources.cc
  ${EURORACK_DIR}/marbles/resources.cc
  ${EURORACK_DIR}/clouds/resources.cc
  ${EURORACK_DIR}/clouds/dsp/correlator.cc
  ${EURORACK_DIR}/clouds/dsp/granular_processor.cc
  ${EURORACK_DIR}/clouds/dsp/mu_law.cc
  ${EURORACK_DIR}/clouds/dsp/pvoc/frame_transformation.cc
  ${EURORACK_DIR}/clouds/dsp/pvoc/phase_vocoder.cc
  ${EURORACK_DIR}/clouds/dsp/pvoc/stft.cc
  ${EURORACK_DIR}/marbles/ramp/ramp_extractor.cc
  ${EURORACK_DIR}/marbles/random/t_generator.cc
  ${EURORACK_DIR}/peaks/modulations/multistage_envelope.cc
  ${EURORACK_DIR}/stmlib/dsp/atan.cc
  ${EURORACK_DIR}/stmlib/dsp/units.cc
  ${EURORACK_DIR}/stmlib/utils/random.cc)
# The shims directory comes first so that Grids' AVR headers resolve to it, then
# the header overrides (see patches/README.md), then the submodule.
target_include_directories(mutable_dsp SYSTEM PUBLIC
  ${CMAKE_SOURCE_DIR}/shims
  ${CMAKE_SOURCE_DIR}/patches
  ${EURORACK_DIR})
target_compile_definitions(mutable_dsp PUBLIC TEST)
# plaits/user_data.h calls printf in TEST mode without including stdio.
target_compile_options(mutable_dsp PRIVATE -O2 -w -include cstdio)

# --- Core: model, sequencer, audio engine (no GUI dependency) ---------------

find_package(nlohmann_json 3 QUIET)
if(NOT nlohmann_json_FOUND)
  FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
  FetchContent_MakeAvailable(json)
endif()

add_library(drumfake_core STATIC
  src/core/params.cc
  src/core/presets.cc
  src/core/scales.cc
  src/core/settings.cc
  src/core/project.cc
  src/core/project_io.cc
  src/core/grids_gen.cc
  src/core/marbles_gen.cc
  src/core/midi_parser.cc
  src/core/sample_file.cc
  src/core/sample_pool.cc
  src/core/slicing.cc
  src/audio/clouds_fx.cc
  src/audio/mod_sources.cc
  src/audio/grain_player.cc
  src/audio/sample_player.cc
  src/audio/sample_loader.cc
  src/audio/track_voice.cc
  src/audio/master_fx.cc
  src/audio/engine.cc
  src/audio/wav_render.cc)
target_include_directories(drumfake_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(drumfake_core PUBLIC mutable_dsp nlohmann_json::nlohmann_json)
target_compile_options(drumfake_core PRIVATE -Wall -Wextra -O2)

# --- Tests -------------------------------------------------------------------

if(NOT ANDROID)
  add_executable(drumfake_tests tests/tests.cc)
  target_link_libraries(drumfake_tests PRIVATE drumfake_core)
  if(EMSCRIPTEN)
    # Runs under Node.js: node build-web/drumfake_tests.js
    target_link_options(drumfake_tests PRIVATE -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=4MB)
  endif()
  enable_testing()
  add_test(NAME drumfake_tests COMMAND drumfake_tests)
endif()

# --- GUI application ---------------------------------------------------------

if(ANDROID)
  # Android (built by android/app/build.gradle): SDL3 from the submodule, and
  # the app as libmain.so, loaded by SDLActivity.
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
elseif(EMSCRIPTEN)
  # Web (emcmake): SDL3 from the submodule, linked statically.
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
elseif(DRUMFAKE_GUI)
  find_package(SDL3 REQUIRED CONFIG)
endif()

if(ANDROID OR EMSCRIPTEN OR DRUMFAKE_GUI)

  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(imgui)

  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  if(ANDROID)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
    target_link_libraries(imgui PUBLIC SDL3::SDL3 GLESv3)
  elseif(EMSCRIPTEN)
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
    target_link_libraries(imgui PUBLIC SDL3::SDL3)
  else()
    # No OpenGL link: the ImGui backend loads GL at run time (see app.cc).
    target_link_libraries(imgui PUBLIC SDL3::SDL3 ${CMAKE_DL_LIBS})
  endif()

  set(APP_SOURCES
    src/main.cc
    src/app.cc
    src/io/midi_common.cc
    src/ui/widgets.cc
    src/ui/theme.cc
    src/ui/faceplate.cc)
  if(ANDROID)
    list(APPEND APP_SOURCES src/io/midi_android.cc)
  elseif(EMSCRIPTEN)
    list(APPEND APP_SOURCES src/io/midi_web.cc)
  else()
    # RtMidi (ALSA) for the desktop MIDI inputs.
    set(RTMIDI_API_JACK OFF CACHE BOOL "" FORCE)
    set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(RTMIDI_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    FetchContent_Declare(rtmidi
      GIT_REPOSITORY https://github.com/thestk/rtmidi.git
      GIT_TAG 6.0.0
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(rtmidi)
    list(APPEND APP_SOURCES src/io/midi_rtmidi.cc)
  endif()
  if(ANDROID)
    add_library(main SHARED ${APP_SOURCES})
    set(APP_TARGET main)
    target_link_libraries(main PRIVATE android log)
  else()
    add_executable(drumfake ${APP_SOURCES})
    set(APP_TARGET drumfake)
  endif()
  target_link_libraries(${APP_TARGET} PRIVATE drumfake_core imgui SDL3::SDL3)
  if(EMSCRIPTEN)
    # drumfake.html + .js + .wasm + .data (the fonts), see docs/building.md.
    set_target_properties(drumfake PROPERTIES SUFFIX ".html")
    set(WEB_DIR ${CMAKE_SOURCE_DIR}/web)
    target_link_options(drumfake PRIVATE
      -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sGL_ENABLE_GET_PROC_ADDRESS
      -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=4MB
      -sFORCE_FILESYSTEM -lidbfs.js
      --pre-js ${WEB_DIR}/pre.js
      --shell-file ${WEB_DIR}/shell.html
      --preload-file ${CMAKE_SOURCE_DIR}/android/app/src/main/assets/fonts@/fonts)
    set_property(TARGET drumfake APPEND PROPERTY LINK_DEPENDS
      ${WEB_DIR}/pre.js ${WEB_DIR}/shell.html)
  elseif(NOT ANDROID)
    target_link_libraries(${APP_TARGET} PRIVATE rtmidi)
  endif()
  target_compile_options(${APP_TARGET} PRIVATE -Wall -Wextra)
else()
  add_executable(drumfake src/main.cc)
  target_compile_definitions(drumfake PRIVATE DRUMFAKE_HEADLESS)
  target_link_libraries(drumfake PRIVATE drumfake_core)
endif()
