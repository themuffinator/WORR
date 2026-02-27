# Vulkan Native World Pass Bootstrap (2026-02-10)

## Context
The native `vulkan` renderer had UI/menu/console rendering, but no 3D world draw path. `R_RenderFrame` was effectively a no-op, so map loads succeeded while nothing was rendered for world geometry.

This change introduces the first native Vulkan world pass so map geometry is submitted and drawn without any OpenGL redirection.

## Root Cause
- `src/rend_vk/vk_main.c` had no world registration/render integration:
  - `R_BeginRegistration`, `R_EndRegistration`, `R_RenderFrame`, and `R_LightPoint` were stubs.
  - Command recording only called `VK_UI_Record`.
- The Vulkan swapchain render pass lacked depth attachment setup needed for proper 3D rendering.
- A world pipeline/module did not exist in the native Vulkan backend wiring.

## Implementation Summary

### 1) Native Vulkan world module
- Added `src/rend_vk/vk_world.h`.
- Added `src/rend_vk/vk_world.c`.
- Added `src/rend_vk/vk_world_spv.h` (embedded SPIR-V for initial world shaders).

Module responsibilities:
- Load BSP world in `VK_World_BeginRegistration` using `BSP_Load("maps/<name>.bsp")`.
- Build triangle mesh from BSP faces/surfedges for drawable surfaces.
- Upload mesh to a Vulkan vertex buffer.
- Build a native graphics pipeline for world rendering.
- Accept per-frame camera state from `refdef_t` and record world draw calls.

Current world shading is intentionally minimal (flat face color bootstrap) to establish reliable geometry submission and frame flow before texture/lightmap/lightgrid stages are layered on.

### 2) Depth-enabled swapchain/renderpass
- Extended swapchain state in `src/rend_vk/vk_local.h`:
  - `depth_format`, `depth_image`, `depth_memory`, `depth_view`.
- Updated swapchain creation/destruction in `src/rend_vk/vk_main.c`:
  - Chosen supported depth format.
  - Created depth image + memory + image view.
  - Updated render pass to color + depth attachments.
  - Updated framebuffer attachments to include depth.

### 3) Renderer lifecycle and frame wiring
- `src/rend_vk/vk_main.c` now initializes/shuts down world subsystem:
  - `VK_World_Init` during `R_Init`.
  - `VK_World_Shutdown` during context teardown.
- Swapchain recreation now rebuilds world resources:
  - `VK_World_CreateSwapchainResources`.
  - `VK_World_DestroySwapchainResources`.
- Renderer API entry points now delegate to world module:
  - `R_BeginRegistration` -> `VK_World_BeginRegistration`
  - `R_EndRegistration` -> `VK_World_EndRegistration`
  - `R_RenderFrame` -> `VK_World_RenderFrame`
  - `R_LightPoint` -> `VK_World_LightPoint`
- Command recording order now includes world pass before UI:
  - `VK_World_Record(...)`
  - `VK_UI_Record(...)`
- Render pass clear values now include both color and depth.

### 4) Build integration
- Updated `meson.build` `renderer_vk_src` list to include:
  - `src/rend_vk/vk_world.c`
  - `src/rend_vk/vk_world.h`
  - `src/rend_vk/vk_world_spv.h`

## Validation Performed

### Build
- Command: `meson compile -C builddir`
- Result: success, `worr_vulkan_x86_64.dll` built and linked.

### Runtime smoke test
- Command:
  - `builddir\\worr.exe +set r_renderer vulkan +set developer 1 +map q2dm1 +wait ... +quit`
- Result:
  - Vulkan renderer initialized.
  - Map `q2dm1` loaded.
  - New world logs confirm world registration and draw submission:
    - `VK_World_BeginRegistration: map=q2dm1 vertices=29805`
    - `VK_World_Record: rendered map=q2dm1 vertices=29805`

## Known Gaps (Next Steps)
- This is not yet legacy GL-parity shading.
- Remaining native Vulkan tasks for `vk` renderer parity:
  - world texture sampling
  - static lightmaps
  - dynamic lights blending
  - lightgrid sampling for entities
  - translucent sorting/paths
  - sky/warp materials
  - model rendering path integration (MD2/MD3/etc.)

This bootstrap intentionally establishes stable 3D frame plumbing first so subsequent lighting/material passes can be implemented incrementally and verified in isolation.
