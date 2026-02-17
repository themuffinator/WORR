# Vulkan Native Lightmap + Lightgrid Sampling (2026-02-10)

## Summary
Native `vulkan` world rendering now samples BSP lighting data instead of using purely normal-based fallback coloring.

This update adds:
- per-vertex BSP lightmap sampling during world mesh build
- `R_LightPoint` world lighting from lightgrid/lightmaps/dlights
- style-aware light contributions (`lightstyles`) for sampled data

All changes are native in `rend_vk` and do not redirect renderer paths to OpenGL.

## What Changed

### 1) World mesh lighting now uses BSP lightmaps
File: `src/rend_vk/vk_world.c`

- Added lightmap helpers:
  - `VK_World_LightStyleWhite`
  - `VK_World_ShiftLightmapBytes`
  - `VK_World_AdjustLightColor`
  - `VK_World_ColorFromLight`
  - `VK_World_SampleFaceLightmap`
  - `VK_World_SampleSurfacePointLight`
- Updated `VK_World_BuildMesh` to sample each triangle vertex from BSP face lightmaps.
- Preserved fallback face shading when a sample is unavailable.
- Added debug metric log for lighting coverage:
  - `VK_World_BuildMesh: vertices=<n> lightmapped=<n>`

Result on `q2dm1`: all generated world vertices were successfully lightmapped in runtime log.

### 2) `R_LightPoint` now follows world BSP lighting data
File: `src/rend_vk/vk_world.c`

`VK_World_LightPoint` now:
- tries octree lightgrid first (`BSP_LookupLightgrid`) with trilinear interpolation
- falls back to BSP lightmap ray sample (`BSP_LightPoint`) when needed
- adds dynamic light influence from current `refdef` dlights
- clamps final light values to a safe range

This aligns behavior with expected legacy-style renderer lighting queries used by game/client logic.

### 3) Frame context for style/dlight-aware lighting
File: `src/rend_vk/vk_world.c`

- Added `current_fd` storage in world state.
- `VK_World_RenderFrame` stores current frame refdef.
- `VK_World_ClearFrame` clears the frame pointer.

This enables lightstyle + dlight context for both world sampling and lightpoint queries.

## Validation

### Build
- `meson compile -C builddir` passed.

### Vulkan runtime smoke
- Command:
  - `builddir\\worr.exe +set r_renderer vulkan +set developer 1 +map q2dm1 +wait ... +quit`
- Result:
  - map loaded and rendered without assertion/fatal error
  - logs showed:
    - `VK_World_BuildMesh: vertices=29805 lightmapped=29805`
    - `VK_World_Record: rendered map=q2dm1 vertices=29805`

### OpenGL regression smoke
- `builddir\\worr.exe +set r_renderer opengl +map q2dm1 +quit` passed.

## Current Status vs Target

This delivers native Vulkan lightmap/lightgrid sampling behavior for world color and light queries.

Still pending for full non-RTX GL-style parity:
- textured world material pass (diffuse texture sampling + proper modulation)
- dynamic light application in world surface shading pass
- full model rendering integration under native Vulkan
