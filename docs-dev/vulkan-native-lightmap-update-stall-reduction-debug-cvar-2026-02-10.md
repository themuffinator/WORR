# Vulkan Native Lightmap Update Stall Reduction + Debug Cvar (2026-02-10)

## Objective
Continue refining native Vulkan world lightmap updates by:
- reducing unnecessary synchronization stalls during atlas updates
- adding explicit runtime diagnostics control for lightmap dirty updates

## Root Cause
- `src/rend_vk/vk_ui.c` performed `vkDeviceWaitIdle` before every image pixel update path in `VK_UI_SetImagePixels(...)`, even when no resource recreation was needed.
- `VK_UI_UpdateImageRGBASubRect(...)` also used `vkDeviceWaitIdle`, adding extra full-device stalls on each lightstyle-driven dirty upload.
- World lightmap dirty upload stats were only available through `Com_DPrintf`, which depends on developer logging and was not cvar-controlled.

## Implementation

### 1) Removed unnecessary full-device idles in hot update paths
- File: `src/rend_vk/vk_ui.c`
- `VK_UI_SetImagePixels(...)`:
  - moved `vkDeviceWaitIdle` into the `needs_recreate` branch only.
  - no idle is done for same-size image updates.
- `VK_UI_UpdateImageRGBASubRect(...)`:
  - removed `vkDeviceWaitIdle` before sub-rect upload.
- Safety rationale:
  - upload commands still run through immediate command submission and end with queue idle in `VK_UI_EndImmediate(...)`.
  - full device idle remains for resource recreation/destruction path where it is required.

### 2) Added Vulkan cvar-controlled lightmap diagnostics
- File: `src/rend_vk/vk_world.c`
- Added cvar:
  - `vk_lightmap_debug` (default `0`)
- Init:
  - created in `VK_World_Init(...)` via `Cvar_Get("vk_lightmap_debug", "0", 0)`.
- Behavior:
  - when `vk_lightmap_debug 1`: prints lightstyle update summary (styles changed, faces updated, dirty rect, atlas coverage).
  - when `vk_lightmap_debug 2`: also prints when styles changed but no faces referenced those styles.

## Validation
- Build:
  - `meson compile -C builddir` (success)
- Runtime smoke:
  - Vulkan: `worr.exe +set vid_renderer vulkan +set r_renderer vulkan +set vk_lightmap_debug 1 +set developer 1 +map q2dm1 +wait 240 +quit` (exit code 0)
  - OpenGL regression smoke: `worr.exe +set vid_renderer opengl +set r_renderer opengl +map q2dm1 +wait 120 +quit` (exit code 0)

## Notes
- All changes are native to `rend_vk` and do not redirect rendering to OpenGL.
- This keeps behavior intact while reducing synchronization overhead during dynamic lightstyle updates.
