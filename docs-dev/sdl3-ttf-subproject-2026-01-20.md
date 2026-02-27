SDL3_ttf Subproject Fallback

Overview
- Added Meson wrapdb subprojects for SDL3 (3.4.0) and SDL3_ttf (3.2.2).
- The build now falls back to these subprojects when system SDL3/SDL3_ttf
  are not available, enabling `USE_SDL3_TTF` and real TTF rendering.

What Changed
- `meson.build` now uses `fallback` for `sdl3` and `sdl3_ttf` dependencies.
- New wrap files:
  - `subprojects/sdl3.wrap`
  - `subprojects/sdl3_ttf.wrap`

How It Resolves TTF Fonts
- When SDL3_ttf is missing, Meson downloads and builds the subproject instead
  of disabling TTF support. This flips `USE_SDL3_TTF` to `1` and removes the
  "SDL3_ttf not available" fallback path at runtime.

Notes
- Use `-Dsdl3-ttf=enabled` if you want configuration to fail rather than
  silently disable TTF when dependencies are missing.
- If building offline, pre-populate `subprojects/packagecache` or set
  `wrap_mode=nodownload` after fetching the wrap sources once.
