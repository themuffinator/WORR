# Vulkan Sky Re-Registration Crash Fix - 2026-02-27

Task IDs: `FR-01-T04`

## Summary
Fixed a Vulkan crash that could occur when reloading a level (especially when toggling `vk_md5_use` and reloading).  
The crash manifested in `VK_World_Record` during skybox draw (`vkCmdDraw`) due to stale sky descriptor/image handles.

## Root Cause
`VK_World_SetSky` previously:
1. Registered new sky face images first.
2. Then called `VK_World_UnsetSky` (which unregisters current sky images/descriptors).
3. Then assigned the "new" handles.

On same-map reloads, `VK_UI_RegisterImage` often returned existing handles for the same sky face paths.  
That meant the "new" handles were actually the current handles, and `VK_World_UnsetSky` destroyed them before re-assignment.  
Result: stale descriptor sets were later bound in sky draw, causing a driver-side access violation.

## Implementation

File: `src/rend_vk/vk_world.c`

- Updated `VK_World_SetSky` ordering to prevent handle alias invalidation:
  - Detect whether sky images are currently present.
  - If present, wait for device idle before image lifetime changes.
  - Clear old sky state first (`VK_World_UnsetSky`).
  - Register/bind new sky images after old handles are gone.
- Applied the same idle+unset protection for the empty/disabled sky path.

This ensures any reused path registrations are recreated cleanly and never invalidated by subsequent unset operations.

## Validation

- Build: `meson compile -C builddir` succeeded.
- Staging: `python tools/refresh_install.py --build-dir builddir` succeeded.
- Repro flow (same map reload with MD2->MD5 toggle) completed without crash:
  - `.install/worr.exe +set developer 1 +set logfile 2 +set r_renderer vulkan +set vk_md5_use 0 +map q2dm1 +set vk_md5_use 1 +map q2dm1 +quit`
- Log confirms two map spawns and two sky-set operations in one run without renderer crash.
