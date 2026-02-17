# Vulkan RTX Cvar Prefix Migration + `rtx_enable 0` Architecture Notes (2026-02-10)

## Summary
This change migrates Vulkan RTX-mode cvar naming from `vk_` to `rtx_` for RTX-specific toggles, while preserving compatibility aliases for existing configs and launch arguments.

It also records why `rtx_enable 0` does not currently match GL legacy rendering and why a split renderer architecture is recommended.

## Implemented Changes

### 1) Renamed RTX mode cvars to `rtx_` prefix
Primary cvars in vkpt are now:
- `rtx_enable` (was `vk_rtx`)
- `rtx_lightmaps` (was `vk_lightmaps`)
- `rtx_lightgrid` (was `vk_lightgrid`)

Source:
- `src/rend_vk/vkpt/main.c`
- `src/rend_vk/vkpt/draw.c`
- `src/rend_vk/vkpt/vkpt.h`

### 2) Added legacy alias synchronization
Legacy cvars are still created and synchronized bidirectionally to avoid breaking existing startup scripts and user cfgs:
- `vk_rtx` <-> `rtx_enable`
- `vk_lightmaps` <-> `rtx_lightmaps`
- `vk_lightgrid` <-> `rtx_lightgrid`

Legacy aliases are marked `CVAR_NOARCHIVE` and synced through a shared alias callback.

### 3) Updated launch config
VSCode Vulkan launch now sets:
- `+set rtx_enable 0`

Source:
- `.vscode/launch.json`

### 4) Updated project cvar naming rules
`AGENTS.md` updated to explicitly define `rtx_` as the preferred prefix for Vulkan RTX/path-tracing mode controls.

Source:
- `AGENTS.md`

## Validation

Build:
- `meson compile -C builddir` succeeded.

Runtime smoke:
- `worr.exe +set r_renderer vulkan +set rtx_enable 0 +map q2dm1 +quit` succeeded.
- `worr.exe +set r_renderer vulkan +set vk_rtx 0 +map q2dm1 +quit` succeeded (legacy alias path).

## Why `rtx_enable 0` still does not equal GL legacy rendering
Current Vulkan renderer (`vkpt`) is architecturally a ray/path-traced pipeline.

`rtx_enable 0` currently disables many RTX-heavy behaviors (accumulation/denoiser/reflections), but world shading still runs through vkpt’s ray-based pipeline. That is not the same rendering model as GL’s classic raster + multitexture lightmap pass.

Key mismatch:
- GL legacy path uses dedicated raster world draw with lightmap atlases and GL state/pipeline composition.
- vkpt path uses ray-based hit/material/light evaluation and does not currently expose a full GL-equivalent raster lightmap stage.

## Recommendation: split `rtx_enable 0` into a dedicated Vulkan legacy renderer path
Yes, splitting is recommended.

Suggested architecture:
1. Keep current vkpt renderer for `rtx_enable 1`.
2. Add a dedicated native Vulkan legacy/raster backend for `rtx_enable 0` (or a separate renderer id, e.g. `vulkan_legacy`).
3. Share renderer-agnostic math/UI/cvar glue in `src/renderer/`.

Benefits:
- Clear separation of rendering paradigms.
- Enables true GL-style parity target (lightmaps/lightgrid/shadowmap behavior) without fighting vkpt assumptions.
- Reduces complexity of mode-specific conditionals inside vkpt.

## Next Implementation Phase (not yet implemented in this patch)
- Build a Vulkan raster world pass with:
  - BSP lightmap atlas generation/update
  - base texture + lightmap combine path
  - GL-parity dynamic lights and lightgrid usage
  - GL-parity shadowmap integration for legacy mode
- Route `rtx_enable 0` to that native Vulkan legacy path.
