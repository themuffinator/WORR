# Vulkan Non-RTX Render Restore (2026-02-10)

## Problem
After splitting renderer names into `opengl`, `vulkan`, and `vulkan_rtx`, selecting:

- `r_renderer vulkan`

produced a black frame ("nothing draws").

Root cause:

- `worr_vulkan_<cpu>.dll` was built from `src/rend_vk/vk_main.c`.
- `vk_main.c` is currently a bootstrap path that clears the swapchain but does not implement full world/UI draw submission yet.

## Fix
File: `meson.build`

1. Keep explicit split outputs:
   - `worr_vulkan_<cpu>.<ext>`
   - `worr_vulkan_rtx_<cpu>.<ext>`
2. Build `worr_vulkan_rtx` from vkpt sources with:
   - `-DRENDERER_VULKAN_RTX=1`
3. Build `worr_vulkan` from the same functional vkpt renderer source set for now (temporary bridge), with:
   - `-DRENDERER_VULKAN_LEGACY=1`
4. Keep both targets native Vulkan (no OpenGL redirection).

This immediately restores visible rendering for the new `vulkan` renderer while vkQuake2-style raster-port work continues in `vk_main.c`.

## Mode Default Difference
File: `src/rend_vk/vkpt/main.c`

- `rtx_enable` default now depends on build flavor:
  - `vulkan_rtx` build: default `rtx_enable = 1`
  - `vulkan` build (`RENDERER_VULKAN_LEGACY`): default `rtx_enable = 0`

This preserves a practical behavioral distinction between the two Vulkan renderer names even while sharing the current functional pipeline.

## Validation
`meson compile -C builddir`:

- built `worr_vulkan_x86_64.dll`
- built `worr_vulkan_rtx_x86_64.dll`

Runtime smoke tests:

1. `worr.exe +set r_renderer vulkan +map q2dm1 +quit`
2. `worr.exe +set r_renderer vulkan_rtx +map q2dm1 +quit`

Both paths now render/load map startup successfully instead of black output.

## Attribution Note
Reference for ongoing native Vulkan raster path work:

- https://github.com/kondrak/vkQuake2

Current change is a render-restore bridge, not completion of vkQuake2 parity port.
