# Vulkan UI Scaling Parity + Shared Renderer Refactor (2026-02-10)

## Summary
This change ports the OpenGL UI scale/aspect behavior to the vkpt Vulkan renderer and moves shared UI scaling logic into the common renderer layer.

## Why this was needed
Vulkan was using a simplified UI scale model (`1.0 / clamp(cvar, 1..10)`) and 2D NDC conversion based directly on `r_config.width/height`. OpenGL uses:
- integer base scale derived from virtual 640x480
- integer UI scale derived from base scale and cvar
- virtual dimensions (`virtual_width`, `virtual_height`) for 2D projection

That mismatch causes UI scale/aspect drift between GL and Vulkan, especially on non-4:3 resolutions and custom UI scale values.

## Shared renderer module
Added shared UI-scale helpers:
- `inc/renderer/ui_scale.h`
- `src/renderer/ui_scale.c`

API added:
- `R_UIScaleBaseInt`
- `R_UIScaleIntForCvar`
- `R_UIScaleClamp`
- `R_UIScaleCompute`
- `R_UIScalePixelRectToVirtual`
- `R_UIScaleClipToPixels`

These centralize base-scale math, clamp behavior, virtual screen derivation, and clip conversion.

## OpenGL refactor
### `src/rend_gl/draw.c`
- Replaced local pixel->virtual conversion with shared `R_UIScalePixelRectToVirtual`.
- Replaced clip rect pixel conversion/clamp with shared `R_UIScaleClipToPixels`.
- Replaced local `R_ClampScale` math with shared `R_UIScaleClamp`.

### `src/rend_gl/state.c`
- Replaced local 2D view base/virtual calculations with shared `R_UIScaleCompute`.

### `src/rend_gl/main.c`
- Replaced local post-process helper pixel->virtual conversion with shared `R_UIScalePixelRectToVirtual`.

## Vulkan vkpt parity changes
### `src/rend_vk/vkpt/main.c`
- `R_ClampScale` now uses shared `R_UIScaleClamp`, matching GL scale semantics.

### `src/rend_vk/vkpt/vkpt.h`
- Expanded `drawStatic_t` with:
  - `base_scale`
  - `virtual_width`
  - `virtual_height`

### `src/rend_vk/vkpt/draw.c`
- Added virtual-metric refresh from shared helpers.
- Changed 2D NDC conversion to use `virtual_width * draw.scale` / `virtual_height * draw.scale` instead of raw framebuffer dimensions.
- `R_SetClipRect` now uses GL-equivalent clip scaling/clamping path via shared helper.

## Build integration
### `meson.build`
Added `src/renderer/ui_scale.c` to both:
- `renderer_src`
- `renderer_vk_src`

This ensures both external renderer DLLs compile against the same shared UI-scale implementation.

## Validation
1. Build:
- Command: `meson compile -C builddir`
- Result: success; both `worr_opengl_x86_64.dll` and `worr_vulkan_x86_64.dll` linked.

2. Runtime Vulkan map load test:
- Command:
  - `builddir\\worr.exe +set r_renderer vulkan +set logfile 1 +set logfile_name vk_ui_scale_q2dm1 +set developer 1 +set deathmatch 1 +set cheats 1 +map q2dm1 +quit`
- Result:
  - Vulkan device initialized
  - server spawned `q2dm1`
  - client completed map load path without assertion/fatal error
  - console log written to:
    - `C:\Users\djdac\Saved Games\Nightdive Studios\Quake II\baseq2\logs\vk_ui_scale_q2dm1.log`

## Notes
- Existing unrelated local edits were preserved.
- This change intentionally keeps behavior-compatible GL results while bringing Vulkan UI scale/aspect behavior in line with GL.
