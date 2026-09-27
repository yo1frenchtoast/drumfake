cmake_minimum_required(VERSION 3.20)
project(drumfreake CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(DRUMFREAKE_GUI "Build the SDL3/ImGui front-end" ON)

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
  ${EURORACK_DIR}/stmlib/dsp/units.cc
  ${EURORACK_DIR}/stmlib/utils/random.cc)
# The shims directory comes first so that Grids' AVR headers resolve to it.
target_include_directories(mutable_dsp SYSTEM PUBLIC
  ${CMAKE_SOURCE_DIR}/shims
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

add_library(drumfreake_core STATIC
  src/core/params.cc
  src/core/presets.cc
  src/core/project.cc
  src/core/project_io.cc
  src/core/grids_gen.cc
  src/audio/track_voice.cc
  src/audio/master_fx.cc
  src/audio/engine.cc
  src/audio/wav_render.cc)
target_include_directories(drumfreake_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(drumfreake_core PUBLIC mutable_dsp nlohmann_json::nlohmann_json)
target_compile_options(drumfreake_core PRIVATE -Wall -Wextra -O2)

# --- Tests -------------------------------------------------------------------

add_executable(drumfreake_tests tests/tests.cc)
target_link_libraries(drumfreake_tests PRIVATE drumfreake_core)
enable_testing()
add_test(NAME drumfreake_tests COMMAND drumfreake_tests)

# --- GUI application ---------------------------------------------------------

if(DRUMFREAKE_GUI)
  find_package(SDL3 REQUIRED CONFIG)

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
  # No OpenGL link: the ImGui backend loads GL at run time (see app.cc).
  target_link_libraries(imgui PUBLIC SDL3::SDL3 ${CMAKE_DL_LIBS})

  add_executable(drumfreake
    src/main.cc
    src/app.cc
    src/ui/widgets.cc
    src/ui/faceplate.cc)
  target_link_libraries(drumfreake PRIVATE drumfreake_core imgui SDL3::SDL3)
  target_compile_options(drumfreake PRIVATE -Wall -Wextra)
else()
  add_executable(drumfreake src/main.cc)
  target_compile_definitions(drumfreake PRIVATE DRUMFREAKE_HEADLESS)
  target_link_libraries(drumfreake PRIVATE drumfreake_core)
endif()
