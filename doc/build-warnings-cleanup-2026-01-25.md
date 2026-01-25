# Build warnings cleanup (2026-01-25)

## Summary
- Removed unused variable and unused function warnings in client UI/font code.
- Suppressed Windows CRT deprecation warnings in OpenAL-Soft for clang toolchains.

## Changes
- `src/client/font.cpp`
  - Guarded TTF-only spacing/kerning state under `USE_SDL3_TTF`.
  - Marked legacy/kfont draw helpers as `[[maybe_unused]]` when TTF/Harfbuzz are disabled.
- `src/client/screen.cpp`
  - Removed an unused `len` variable in chat cursor hit-testing.
- `meson.build`
  - Restored the OpenAL fallback options to the shared static-lib defaults.
- `subprojects/openal-soft-1.23.1/meson.build`
  - Apply `_CRT_SECURE_NO_WARNINGS` on Windows for all compilers, not only MSVC.

## Validation
- Reconfigure via `tools/meson_setup.ps1` so `WINDRES` is set for subprojects.
- Rebuild `builddir` and confirm no warnings are emitted.
