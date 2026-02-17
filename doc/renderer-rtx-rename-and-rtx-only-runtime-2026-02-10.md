# Renderer Identity Cleanup: `vulkan_rtx` -> `rtx` and RTX-Only Runtime (2026-02-10)

## Summary
This change removes ambiguous renderer/mode naming and makes the RTX renderer unambiguously ray-traced:

- Renderer id `vulkan_rtx` was renamed to `rtx`.
- Runtime toggle `rtx_enable` was removed from vkpt renderer behavior.
- `r_renderer rtx` now always runs the RTX path.

## Why `vulkan` currently looks like RTX
The current `vulkan` module is still a bootstrap path built from vkpt source files while native vkQuake2-style raster Vulkan porting is in progress.

Current build wiring in `meson.build`:
- `worr_vulkan_<cpu>` is built from `renderer_vk_rtx_src` (vkpt source group).
- `worr_rtx_<cpu>` is also built from `renderer_vk_rtx_src`.

Because both renderer modules currently share vkpt rendering code, they can look visually similar until the native non-RTX Vulkan path is fully ported and switched over.

## Implemented Changes

### 1) Renderer id rename in client selection
File: `src/client/renderer.cpp`

- Canonical RTX renderer id is now `rtx`.
- Legacy aliases still normalize to `rtx`:
  - `vkpt`
  - `vk_rtx`
  - `vulkan_rtx`
  - `vulkan rtx`
  - `vulkan-rtx`
  - `vulkanrtx`
- Renderer autocompletion now suggests:
  - `opengl`
  - `vulkan`
  - `rtx`

### 2) Renderer DLL name rename
File: `meson.build`

- External RTX DLL target renamed from:
  - `worr_vulkan_rtx_<cpu>`
- to:
  - `worr_rtx_<cpu>`

This matches the new canonical `r_renderer rtx` name and avoids Vulkan-vs-RTX identity overlap.

### 3) Removed runtime RTX mode switch
Files:
- `src/rend_vk/vkpt/main.c`
- `src/rend_vk/vkpt/vkpt.h`

Removed `rtx_enable` runtime behavior and its `vk_rtx` alias plumbing:
- Deleted `cvar_rtx_enable` registration and alias sync entry.
- Deleted `vk_rtx_changed()` callback.
- Removed the `evaluate_reference_mode()` early path that disabled RTX features when `rtx_enable == 0`.

Result:
- RTX renderer no longer has an on/off mode that changes it into a pseudo-legacy mode.
- `rtx` is now strictly RTX behavior.

### 4) VS Code launch task rename/update
File: `.vscode/launch.json`

- Launch config renamed from `WORR (Vulkan RTX)` to `WORR (RTX)`.
- Renderer argument changed to:
  - `+set r_renderer rtx`
- Removed redundant runtime toggle arguments:
  - `+set rtx_enable 1`

## Behavioral Outcome
- `r_renderer rtx`: always ray-traced vkpt behavior.
- `r_renderer vulkan`: still a native Vulkan module, but currently compiled from vkpt sources as a temporary bootstrap, so visuals may still resemble RTX until raster-port work is completed.
- `r_renderer opengl`: unchanged.

## Follow-up
To fully differentiate `vulkan` from `rtx` visually and architecturally, switch `worr_vulkan_<cpu>` from vkpt source wiring to the native raster Vulkan implementation path as that port stabilizes.
