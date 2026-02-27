# Vulkan `vk_rtx 0` Lightmap + Lightgrid Lighting Queries (2026-02-10)

## Summary
Implemented native Vulkan-side BSP lightmap and lightgrid sampling for renderer lighting queries in `rend_vk/vkpt`, with no OpenGL redirection.

This change makes `R_LightPoint` in the Vulkan renderer use Quake II BSP lighting data (world + inline BSP models + lightstyles + lightgrid + dynamic lights) instead of returning constant white.

## Problem
`src/rend_vk/vkpt/draw.c` had a stub:
- `R_LightPoint(...)` always returned white.

That broke parity with GL behavior for lighting queries used by gameplay/client logic (for example, `cl.lightlevel` updates), and prevented Vulkan from using lightgrid/lightmap data for point lighting queries.

## Design
Ported the GL-style light sampling flow into Vulkan (`vkpt`) while keeping the implementation renderer-native:

1. World BSP access from vkpt draw code:
- Added `vkpt_get_world_bsp()` accessor in `src/rend_vk/vkpt/main.c`.
- Declared in `src/rend_vk/vkpt/vkpt.h`.

2. Vulkan cvars for query-path control:
- `vk_lightmaps` (default `1`): enable BSP lightmap sampling in Vulkan lighting queries.
- `vk_lightgrid` (default `1`): enable BSP lightgrid interpolation in Vulkan lighting queries.
- `r_fullbright` is respected in Vulkan `R_LightPoint`.

3. Native BSP lighting query path in `src/rend_vk/vkpt/draw.c`:
- Added bilinear lightmap sampling across lightstyles.
- Added lightgrid lookup + trilinear interpolation (`BSP_LookupLightgrid`).
- Added transformed BSP model probing via `BSP_TransformedLightPoint`.
- Added dynamic light contribution consistent with GL-style query semantics.

4. No GL fallback / no renderer redirection:
- All logic is implemented directly in Vulkan `vkpt` code.

## Files Changed
- `src/rend_vk/vkpt/draw.c`
  - Replaced stub `R_LightPoint` with native BSP light query implementation.
  - Added helper functions for:
    - lightstyle weighting
    - lightmap sample decode
    - lightgrid interpolation
    - dynamic light accumulation

- `src/rend_vk/vkpt/main.c`
  - Added cvar globals:
    - `cvar_vk_lightmaps`
    - `cvar_vk_lightgrid`
    - `cvar_r_fullbright`
  - Registered cvars in `R_Init(...)`.
  - Added `vkpt_get_world_bsp()`.

- `src/rend_vk/vkpt/vkpt.h`
  - Added extern declarations for new cvars.
  - Added declaration for `vkpt_get_world_bsp()`.

## Validation
Build:
- `meson compile -C builddir` (success)

Runtime smoke:
- `builddir\worr.exe +set r_renderer vulkan +set vk_rtx 0 +map q2dm1 +quit`
- Vulkan initialized, map loaded, session shut down cleanly (no assertion/fatal in this path).

## Notes
This change implements full BSP lightmap/lightgrid sampling for Vulkan lighting queries (`R_LightPoint`) used by client/gameplay lighting lookups.

The current `vkpt` world shading pipeline remains its own native Vulkan path-tracing/compositing pipeline; this patch does not switch rendering to GL nor add a separate OpenGL-style raster world pass.
