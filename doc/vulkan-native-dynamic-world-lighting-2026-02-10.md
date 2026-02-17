# Vulkan Native Dynamic World Lighting (2026-02-10)

## Objective
Continue native Vulkan world rendering parity by removing "registration-time baked" world lighting and updating surface lighting every frame using current lightstyles and dynamic lights.

## Problem
- Vulkan world mesh colors were computed once during `R_BeginRegistration` and then remained static.
- Animated lightstyles and dynamic lights (`dlights`) could not affect already uploaded world geometry.
- This diverged from expected Quake II behavior where world lighting responds during play.

## Implementation

### 1) Persistent CPU world mesh data
- File: `src/rend_vk/vk_world.c`
- Added persistent CPU storage:
  - `vk_world.cpu_vertices`
  - `vk_world.vertex_meta`
- Added per-vertex metadata type:
  - `vk_world_vertex_meta_t { face_index, fallback_color }`
- Added cleanup:
  - `VK_World_ClearCpuMesh()`

This allows re-lighting vertex colors each frame without rebuilding geometry/batches.

### 2) Mesh build metadata output
- File: `src/rend_vk/vk_world.c`
- Extended mesh build output:
  - `VK_World_BuildMesh(...)` now outputs metadata alongside vertices/batches.
- Every emitted vertex stores:
  - source face index
  - fallback face-derived color.

### 3) Per-frame vertex lighting refresh
- File: `src/rend_vk/vk_world.c`
- Added helpers:
  - `VK_World_AddDynamicLightsAtPoint(...)`
  - `VK_World_ClampLight(...)`
  - `VK_World_UpdateVertexLighting()`
- `VK_World_UpdateVertexLighting()`:
  - samples face lightmaps at each vertex (using current lightstyle values)
  - adds dynamic light contribution from current `refdef_t`
  - clamps and writes final color into `cpu_vertices`
  - uploads refreshed vertex buffer data to Vulkan memory each frame.

### 4) Frame path integration
- File: `src/rend_vk/vk_world.c`
- `VK_World_RenderFrame(...)` now calls `VK_World_UpdateVertexLighting()` before activating the frame draw.
- If vertex lighting update fails, frame activation is skipped safely.

### 5) Registration/shutdown ownership fixes
- File: `src/rend_vk/vk_world.c`
- `VK_World_BeginRegistration(...)` now transfers ownership of built vertices/meta into persistent world state (instead of freeing immediately).
- CPU mesh buffers are cleared when switching maps and during renderer shutdown.

### 6) LightPoint helper consistency
- File: `src/rend_vk/vk_world.c`
- `VK_World_LightPoint(...)` now reuses shared helpers for dlight addition/clamping to match vertex-light update math paths.

## Validation
- Build:
  - `meson compile -C builddir`
- Vulkan runtime smoke:
  - `worr.exe +set vid_renderer vulkan +set r_renderer vulkan +set developer 1 +map q2dm1 ...`
- Observed:
  - `VK_World_BuildMesh: vertices=29805 batches=1230 lightmapped=29805`
  - `VK_World_BeginRegistration: map=q2dm1 vertices=29805 batches=1230`
  - `VK_World_Record: rendered map=q2dm1 vertices=29805 batches=1230`

## Notes
- This is still a vertex-color lighting path (not yet full GPU lightmap texture composition), but it now updates dynamically per-frame with active styles/lights.
- All changes remain native to `rend_vk`; no OpenGL rendering path redirection was introduced.
