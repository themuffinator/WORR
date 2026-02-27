# VKPT Vulkan Port Fixes (2026-01-22)

## Summary
This update closes two gaps from the Q2RTX vkpt port work: rotated 2D UI quads
now render correctly in Vulkan, and SDL video windows now expose platform-native
handles so Vulkan surface creation works on SDL backends without renderer-side
SDL dependencies.

## UI rotation (`R_DrawStretchRotatePic`)
- `R_DrawStretchRotatePic` now enqueues a rotated stretch-pic instance instead
  of falling back to the unrotated path.
- Stretch-pic instances now carry a base vertex and two axes (axis X/Y) in NDC.
  These are derived from pixel-space rotation and then scaled, preserving correct
  rotation even when screen aspect scaling differs.
- Rotated quads use an AABB reject against the clip rect to avoid submitting
  off-screen draws (no UV clipping is applied for rotated quads).

Files:
- `src/rend_vk/vkpt/draw.c`
- `src/rend_vk/vkpt/shader/stretch_pic.vert`

## SDL native handles for Vulkan
- The SDL video driver now maps SDL windows to Win32/X11/Wayland handles via SDL
  window properties.
- `vid->get_native_window` returns `VID_NATIVE_WIN32`/`VID_NATIVE_X11`/
  `VID_NATIVE_WAYLAND` when available, allowing Vulkan surface creation through
  the existing platform paths.

Files:
- `src/unix/video/sdl.c`

## Notes
- Shader updates require regenerating `shader_vkpt` SPIR-V outputs via
  `tools/compile_vkpt_shaders.py` or `meson compile -C builddir` with Vulkan
  enabled.
