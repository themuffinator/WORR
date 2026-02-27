# Vulkan Backend Separation Correction (2026-02-10)

## What Was Wrong
The `vulkan` renderer target was temporarily wired to the vkpt/RTX source group (`renderer_vk_rtx_src`) to keep Vulkan launches running while startup crashes were being debugged.

That made `vulkan` and `rtx` share the same renderer implementation, which is why visuals looked RTX-like and did not satisfy the intended architecture split.

## Why It Happened
The native Vulkan backend file (`src/rend_vk/vk_main.c`) is still an incomplete vkQuake2-style raster port with many rendering stubs (2D/world draw paths are largely no-op). During crash triage, the build was temporarily pointed at vkpt to avoid a non-drawing/black-screen outcome.

That temporary wiring should have been removed sooner.

## Fix Applied
`meson.build` was corrected so `vulkan` now builds from its own native source set:

- Before:
  - `renderer_vk_lib_src = renderer_vk_rtx_src + ['src/renderer/renderer_api.c']`
- After:
  - `renderer_vk_lib_src = renderer_vk_src + ['src/renderer/renderer_api.c']`

Also narrowed include dirs for native Vulkan build to its own requirements.

## Validation
Build and smoke tests performed:

1. `meson compile -C builddir` succeeded.
2. `worr.exe +set r_renderer vulkan +map q2dm1 +quit` succeeded.
3. `worr.exe +set r_renderer rtx +map q2dm1 +quit` succeeded.

Log evidence of renderer split:

- Vulkan log (`vk_separate_smoke.log`) shows native path:
  - `------- VK_Init -------`
  - `Using video driver: win32wgl`
  - `Vulkan device: ...`
- RTX log (`rtx_separate_smoke.log`) shows vkpt path:
  - `----- init_vulkan -----`
  - `Using VK_KHR_ray_query`

## Current Reality
Renderer identity is now correctly split at build/load level:

- `opengl` -> GL renderer
- `vulkan` -> native Vulkan backend (`vk_main.c` path)
- `rtx` -> vkpt RTX backend

However, native `vulkan` rendering parity is not complete yet (expected until vkQuake2 raster port work is finished).

## Next Porting Focus
To complete the native Vulkan renderer:

1. Implement world/brush/model draw paths in `src/rend_vk/vk_main.c` (currently stubbed).
2. Implement textured 2D draw path for menu/console/HUD (`R_Draw*` functions currently mostly no-op).
3. Wire image/model registration and uploads instead of handle-only stubs.
4. Add parity checks against GL for `q2dm1` (menu, console, in-world draw, lightmaps).
