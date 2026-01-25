# Quake Live Renderer Cvars in OpenGL

## Overview
This update introduces Quake Live-style renderer cvars for OpenGL, updates defaults for overbright controls, and adds more granular texture filtering for `r_picmip`.

## New Cvars
### r_map_overbright_cap
- Default: `255`
- Range: `0-255`
- Behavior: Caps shifted map lighting values after overbright adjustment. The cap is applied to lightmap and lightgrid samples during lightmap build and sampling.
- Notes: `255` preserves existing behavior (no additional cap).

### r_picmip_filter
Bitflags that opt out specific image categories from `r_picmip` downscaling.
- Default: `3`
- Flags:
  - `1` = don't affect non-player model skins
  - `2` = don't affect player skins (paths under `players/`)
  - `4` = don't affect world textures
  - `8` = don't affect sky textures
- Interactions: `r_nomip` still forces picmip to world textures only; `gl_downsample_skins` must be enabled for skins to be affected at all.

## Default Changes
- `r_map_overbright_bits`: now defaults to `0` (was `2`).
- `r_overbright_bits`: remains at `1`.

## Notes
- `r_map_overbright_cap`, `r_map_overbright_bits`, and `r_picmip_filter` are CVAR_FILES and require a renderer reload to take effect.
