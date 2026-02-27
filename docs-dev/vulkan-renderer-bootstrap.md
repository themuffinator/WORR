# Vulkan Renderer Bootstrap

## Summary
This update replaces the Vulkan renderer stub with a bootstrap path that can
initialize Vulkan, create a surface + swapchain, and present a cleared frame.
It also wires the renderer to the existing video drivers so the renderer can
load without immediate failures.

## Files Updated
- `src/rend_vk/vk_main.c`: new Vulkan instance/device/swapchain + frame present path.
- `src/rend_vk/vk_local.h`: expanded Vulkan state to track device and swapchain state.

## Initialization Flow
- `R_Init(true)` now:
  - calls `vid->init()` to create the native window.
  - queries native window handles via `vid->get_native_window`.
  - creates the Vulkan instance and surface.
  - selects a physical device + queue family and creates the logical device.
  - creates the swapchain and frame resources if a size is available.
- `R_Shutdown(true)` tears down swapchain resources, the device, surface, and instance,
  then calls `vid->shutdown()`.

## Frame Flow
- `R_BeginFrame` recreates the swapchain if it is marked dirty.
- `R_EndFrame` records a command buffer that clears the swapchain image and presents it.
- Present errors (`VK_ERROR_OUT_OF_DATE_KHR`, `VK_SUBOPTIMAL_KHR`) mark the swapchain dirty
  so it can be rebuilt on the next frame.

## Mode Change Handling
- `R_ModeChanged` updates `r_config`, destroys swapchain resources, recreates the surface
  from the new native window handles, and rebuilds the swapchain.

## Current Limitations
- The renderer still stubs all actual draw paths (models, world, particles, 2D UI).
- Image/texture management and pipeline/shader creation are not implemented yet.
- No validation layer or debug utils integration is enabled.

## Next Implementation Steps
1. Implement image decoding + texture upload (parity with `src/rend_gl/images.c`).
2. Build pipelines for world surfaces, models, particles, and 2D UI.
3. Port lightmap, fog, bloom, and debug draw features.
4. Add validation/debug toggles and any remaining platform-specific surface support gaps.
