# Header overrides

`third_party/eurorack` is never edited. A header placed here shadows the submodule's
one: `patches/` comes before `third_party/eurorack` in the include path of CMakeLists.txt.

- `clouds/dsp/window.h`: `Window::Start()` lost its `done_ = false` in two upstream commits
  ("Duplicate variable assignment", "Fixed double assignments, following static analysis"),
  so the WSOLA windows of Clouds' Stretch mode never start and the mode is silent. The
  line is restored; the rest of the file is unchanged. Remove this override if upstream
  fixes it.
