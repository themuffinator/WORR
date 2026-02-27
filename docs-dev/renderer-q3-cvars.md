# Quake 3 Renderer Cvars in OpenGL

## Overview
This update adds Quake 3-style renderer cvars to the OpenGL backend and wires them to the existing WORR/Q2 rendering paths. It includes new r_* aliases for legacy gl_* cvars, plus lighting and sky behavior that match Quake 3 expectations.

## Cvar Additions and Behavior
- r_picmip (alias): Synced with gl_picmip. Uses the existing picmip path and triggers a renderer reload via CVAR_FILES.
- r_nomip: When enabled, picmip applies only to world (IT_WALL) textures. Skips downsampling for skins.
- r_fastsky: Skips skybox rendering and forces a color clear using gl_clearcolor even if gl_clear is 0. Bmodel sky faces are skipped.
- r_overbright_bits: Controls effective overbright bits. Disabled when hardware gamma is unavailable or when windowed mode is active (unless set negative to bypass the fullscreen clamp). Clamped to 1 for <=16bpp and 2 for >16bpp. Updates identity light scaling used for world and entity modulation.
- r_map_overbright_bits: Controls the overbright bits baked into map/lightgrid data. A shift of (r_map_overbright_bits - r_overbright_bits) is applied to lightmap and lightgrid samples.
- r_lightmap (alias): Synced with gl_lightmap to provide Quake 3 naming for the lightmap-only debug mode.
- r_intensity (alias): Synced with intensity to provide Quake 3 naming for global texture lighting scale.

## Lighting Pipeline Changes
- Added identity_light scaling (1 / (1 << r_overbright_bits)) to world and entity modulate values.
- Lightmap samples (including lightgrid and lightpoint sampling) now apply the Quake 3 overbright shift logic before modulation.
- Gamma table generation applies the overbright shift, matching Quake 3 hardware gamma behavior.

## Notes
- r_picmip, r_nomip, r_overbright_bits, and r_map_overbright_bits are CVAR_FILES and will trigger a renderer restart.
- r_fastsky is runtime and does not restart the renderer.
- gl_* cvars remain supported and are kept in sync with their r_* aliases.
- Overbright and gamma tables are re-evaluated on mode changes so fullscreen/gamma flags stay in sync.
