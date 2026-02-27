# Shadowmapping/Dlight Culling Fixes - 2026-01-28

## Summary
- Removed view-frustum culling from dynamic light UBO uploads so PVS-selected lights are not dropped at the last moment.
- Added a wireframe-only dlight debug mode and HUD stats line to make light selection and shadowing more transparent.

## Technical Changes
- Shader light uploads (`shader.c`, `sp3b.c`) now only cull dlights with non-positive influence radius. This prevents a camera-frustum-only decision from removing lights that still affect visible surfaces.
- Added `SHADOWDBG_DRAW_WIREFRAMES` (`gl_shadow_draw_debug 128`) to draw point-light spheres and spotlight cones without requiring multiple bits.
- Added HUD counters under the FPS readout while `gl_shadow_draw_debug` is active: `dl`, `sh`, `cas`, `dyn`.

## Files
- `src/rend_gl/shader.c`
- `src/rend_gl/sp3b.c`
- `src/rend_gl/main.c`
- `src/game/cgame/cg_draw.cpp`
- `docs-dev/shadowmapping-debug-tools-2026-01-27.md`
