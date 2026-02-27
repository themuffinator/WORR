# Vulkan Native Lightstyle Atlas Refresh (2026-02-10)

## Objective
Continue native `rend_vk` world-lighting parity by adding runtime lightstyle updates for the Vulkan world lightmap atlas, matching legacy behavior where animated lightstyles affect world surfaces during gameplay.

## Root Cause
- The Vulkan world lightmap atlas was generated once at map registration with default style weights.
- Animated styles (flicker/pulse) from `refdef.lightstyles` were not pushed back into the atlas texture after registration.
- Result: world static lighting stayed effectively frozen while lightstyle values changed frame-to-frame.

## Implementation

### 1) Persisted atlas state for runtime updates
- File: `src/rend_vk/vk_world.c`
- Extended world state to keep:
  - packed face lightmap layout table (`face_lms`)
  - CPU RGBA atlas buffer (`lightmap_rgba`)
  - atlas size (`lightmap_atlas_size`)
  - cached lightstyle values (`style_cache`) and validity flag.
- `VK_World_ClearLightmap()` now frees these resources in addition to unregistering the atlas image handle.

### 2) Refactored atlas pixel generation
- File: `src/rend_vk/vk_world.c`
- Added reusable helpers:
  - `VK_World_InitLightmapAtlasPixels(...)`
  - `VK_World_BuildFaceLightmapPixels(...)`
  - `VK_World_RebuildLightmapAtlasPixels(...)`
- `VK_World_BuildLightmapAtlas(...)` now focuses on packing layout and then builds pixels through the reusable path.
- Style weights now come from an explicit `refdef` input path (instead of only implicit current frame state), enabling deterministic map registration and frame refreshes.

### 3) Added runtime lightstyle change detection + atlas upload
- File: `src/rend_vk/vk_world.c`
- Added:
  - `VK_World_CacheLightStyles(...)`
  - `VK_World_LightStylesChanged(...)`
  - `VK_World_UpdateLightmapStyles(...)`
- Per-frame in `VK_World_RenderFrame(...)`:
  - detect lightstyle changes
  - rebuild atlas pixels in CPU buffer only when needed
  - upload via `VK_UI_UpdateImageRGBA(...)`.

### 4) Registration wiring update
- File: `src/rend_vk/vk_world.c`
- `VK_World_BeginRegistration(...)` now stores packed face layout + atlas CPU buffer in world state (instead of freeing them immediately) so runtime updates can reuse layout and avoid re-packing.
- Initial style cache is invalidated so first active frame seeds from live `refdef.lightstyles`.

## Validation
- Build:
  - `meson compile -C builddir` (success)
- Runtime smoke:
  - Vulkan: `worr.exe +set vid_renderer vulkan +set r_renderer vulkan +set developer 1 +map q2dm1 +wait 240 +quit` (exit code 0)
  - OpenGL regression smoke: `worr.exe +set vid_renderer opengl +set r_renderer opengl +map q2dm1 +wait 120 +quit` (exit code 0)

## Notes
- This keeps Vulkan fully native and does not redirect to OpenGL.
- The update path intentionally uploads only when style values change, minimizing unnecessary texture uploads.
