# Vulkan Renderer Split: `vulkan` and `vulkan_rtx` (2026-02-10)

## Summary
WORR renderer selection is now split into three explicit backends:

- `opengl`
- `vulkan` (native non-RTX Vulkan backend target)
- `vulkan_rtx` (current vkpt/Q2RTX backend, renamed from prior `vulkan` target)

This removes ambiguity between Vulkan raster/non-RT work and Vulkan RTX/vkpt work.

## Build Changes
File: `meson.build`

1. Renamed the existing vkpt source group from `renderer_vk_src` to `renderer_vk_rtx_src`.
2. Added a dedicated non-RTX Vulkan source group:
   - `renderer_vk_src` = `src/rend_vk/vk_main.c` + `src/rend_vk/vk_local.h`
3. External renderer outputs now build as:
   - `worr_opengl_<cpu>.<ext>`
   - `worr_vulkan_<cpu>.<ext>`
   - `worr_vulkan_rtx_<cpu>.<ext>`

## Runtime Renderer Selection Changes
File: `src/client/renderer.cpp`

`r_renderer` now supports and advertises:

- `opengl`
- `vulkan`
- `vulkan_rtx`

Normalization/alias behavior:

- `gl` -> `opengl`
- `vk` -> `vulkan`
- `vkpt`, `vk_rtx`, `vulkan rtx`, `vulkan-rtx`, `vulkanrtx`, `rtx` -> `vulkan_rtx`

This preserves practical compatibility with previous Vulkan RTX naming while making `vulkan` unambiguously non-RTX.

## Non-RTX Vulkan Export Coverage
File: `src/rend_vk/vk_main.c`

Added missing renderer API entry points so the non-RTX Vulkan DLL links cleanly:

- `R_RegisterRawImage`
- `R_UnregisterImage`
- `R_DrawStretchSubPic`
- `R_UpdateImageRGBA`

These are currently minimal native Vulkan stubs in `vk_main.c`, establishing a distinct non-RTX backend target without redirecting to OpenGL.

## VSCode Launch Split
File: `.vscode/launch.json`

- `WORR (Vulkan)` now launches with `+set r_renderer vulkan`
- Added `WORR (Vulkan RTX)` launching with `+set r_renderer vulkan_rtx`

## vkQuake2 Credit and Integration Direction
Reference project:

- https://github.com/kondrak/vkQuake2

Attribution:

- The non-RTX Vulkan track is being aligned as a dedicated native renderer path, with `vkQuake2` used as the primary external reference for Quake II Vulkan raster architecture and implementation direction.
- `vkQuake2` is GPL-2.0 licensed; reuse/porting work must retain license and attribution requirements.

Planned follow-up work for `vulkan`:

1. Replace bootstrap stubs in `vk_main.c` with full raster draw path.
2. Port lightmap/lightgrid world shading behavior into native Vulkan raster passes.
3. Add feature parity validation against OpenGL on `q2dm1` and additional maps.
4. Keep `vulkan_rtx` (vkpt) independent from `vulkan` raster implementation.
