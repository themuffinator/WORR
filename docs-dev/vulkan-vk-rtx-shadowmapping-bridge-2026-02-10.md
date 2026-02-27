# Vulkan `vk_rtx 0` Shadowmapping Bridge (2026-02-10)

## Goal
Recreate the OpenGL shadowmapping stack while running with:
- `r_renderer vulkan`
- `vk_rtx 0`

## Context
`vk_rtx 0` intentionally falls back to the OpenGL backend to provide classic raster/lightmap rendering. The missing piece was cvar-level control: Vulkan-mode launches were not exposing a Vulkan-prefixed way to configure GL shadowmap behavior.

## Implementation
File: `src/client/renderer.cpp`

Added a `vk_rtx 0` shadowmapping profile sync:
- Introduced a `vk_*` to `gl_*` alias table for shadow-related controls.
- On renderer selection, if `r_renderer` resolves to Vulkan and `vk_rtx == 0`, engine now syncs the profile before loading OpenGL.

Synced cvars:
- `vk_shadows` -> `gl_shadows`
- `vk_shadowmaps` -> `gl_shadowmaps`
- `vk_shadowmap_size` -> `gl_shadowmap_size`
- `vk_shadowmap_lights` -> `gl_shadowmap_lights`
- `vk_shadowmap_dynamic` -> `gl_shadowmap_dynamic`
- `vk_shadowmap_cache` -> `gl_shadowmap_cache`
- `vk_shadowmap_cache_mode` -> `gl_shadowmap_cache_mode`
- `vk_shadowmap_filter` -> `gl_shadowmap_filter`
- `vk_shadowmap_quality` -> `gl_shadowmap_quality`
- `vk_shadowmap_softness` -> `gl_shadowmap_softness`
- `vk_sun_enable` -> `gl_sun_enable`

Sync policy:
- If `vk_*` was not modified but `gl_*` was modified, copy `gl_*` -> `vk_*`.
- Otherwise copy `vk_*` -> `gl_*`.

This preserves legacy GL user config while allowing Vulkan-mode prefixed control in `vk_rtx 0` mode.

## Validation
1. Build succeeded with `meson compile -C builddir`.
2. Runtime smoke test used:
   - `r_renderer vulkan`
   - `vk_rtx 0`
   - custom `vk_shadowmaps` value
   - queried effective `gl_shadowmaps` at startup
3. Verified fallback message and successful `q2dm1` load path.
