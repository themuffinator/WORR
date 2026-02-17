# Vulkan Native World Lightmap Atlas Pass (2026-02-10)

## Objective
Implement native Vulkan (`rend_vk`) lightmap rendering for `vk_rtx 0` style world shading so world surfaces render with:
- base diffuse texture
- lightmap modulation (legacy-style static lighting)
- lightgrid support preserved for `R_LightPoint` world/model queries

No OpenGL redirection is used.

## Root Cause
- `src/rend_vk/vk_world.c` still had an in-progress refactor:
  - removed `vertex_meta` type/state, but old references remained in per-frame lighting and map registration.
  - world pipeline was still one-texture (set 0 only), so no GPU lightmap sampling path existed.
- world shader inputs/outputs were still `pos + uv + color` and did not carry lightmap UVs.

## Implementation

### 1) Completed native world lightmap atlas creation
- File: `src/rend_vk/vk_world.c`
- Added/finished atlas flow:
  - `VK_World_BuildLightmapAtlas(...)` now packs face lightmaps into a shared RGBA atlas.
  - Uses atlas size fallback candidates: `1024`, `2048`, `4096`.
  - Reserves texel `(0,0)` as white fallback for non-lightmapped faces.
  - Adds a 1-texel border around each packed face block to reduce bilinear bleed between adjacent charts.
  - Stores per-face atlas placement in `vk_world_face_lightmap_t` for runtime LM UV generation.

### 2) Finished mesh generation with per-vertex LM UVs
- File: `src/rend_vk/vk_world.c`
- `vk_world_vertex_t` already carried `lm_uv`; mesh build now fully writes it.
- `VK_World_BuildMesh(...)` signature now takes:
  - `face_lms`
  - `atlas_size`
- Faces without lightmaps map to fallback white texel.

### 3) Added native world lightmap texture lifecycle
- File: `src/rend_vk/vk_world.c`
- Added renderer-owned lightmap state:
  - `lightmap_handle`
  - `lightmap_descriptor_set`
- Added `VK_World_ClearLightmap()` and integrated it into:
  - map re-registration path
  - renderer shutdown path
- During `VK_World_BeginRegistration(...)`:
  - atlas is built
  - atlas is uploaded via `VK_UI_RegisterRawImage("**world_lightmap**", ...)`
  - descriptor set is cached from `VK_UI_GetDescriptorSetForImage(...)`

### 4) Updated Vulkan world pipeline layout and bindings
- File: `src/rend_vk/vk_world.c`
- Pipeline layout now binds two descriptor sets:
  - set `0`: base surface texture
  - set `1`: world lightmap atlas
- Vertex input attributes now include:
  - location 0: `pos`
  - location 1: `uv`
  - location 2: `lm_uv`
  - location 3: `color`
- `VK_World_Record(...)` now binds world lightmap descriptor set at set `1`, and per-batch base textures at set `0`.

### 5) Updated dynamic-light modulation path
- File: `src/rend_vk/vk_world.c`
- Removed stale `vertex_meta` dependencies from `VK_World_UpdateVertexLighting(...)`.
- Per-frame vertex color now starts at white and applies additive dlight modulation only.
- Final fragment color is computed as:
  - `base_texture * lightmap_texture * dynamic_light_modulation`

### 6) Updated world shaders + SPIR-V header
- Files:
  - `.codex_temp/vk_world.vert`
  - `.codex_temp/vk_world.frag`
  - `src/rend_vk/vk_world_spv.h` (regenerated)
- Vertex shader now passes both base UV and lightmap UV to fragment stage.
- Fragment shader now samples two textures (base + lightmap) and multiplies by vertex modulation.

## Validation
- Build:
  - `meson compile -C builddir`
  - result: success (`worr_vulkan_x86_64.dll` rebuilt)
- Runtime smoke command executed:
  - `builddir\\worr.exe +set vid_renderer vulkan +set r_renderer vulkan +map q2dm1 +wait 180 +quit`
  - process exited cleanly with code `0` in local run.

## Notes
- This pass keeps Vulkan native and does not route rendering through OpenGL.
- Lightgrid sampling path (`VK_World_LightPoint`) remains present for model/world lighting queries.
