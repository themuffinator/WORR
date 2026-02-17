# Vulkan `vk_rtx` OpenGL Fallback (2026-02-10)

## Goal
Make `vk_rtx 0` behave like classic lightmap rendering by switching to the OpenGL backend, even when `r_renderer` is set to `vulkan`.

## Root Cause
`vk_rtx` only changed vkpt internal quality/reflection settings in `src/rend_vk/vkpt/main.c`. It did not change the renderer module being loaded, so the game still stayed on vkpt instead of true OpenGL lightmap rendering.

## Implementation
### 1) Engine-side effective renderer selection
File: `src/client/renderer.cpp`

Added an effective renderer resolver:
- Reads `r_renderer` as the requested backend.
- Registers `vk_rtx` in the client with `CVAR_ARCHIVE | CVAR_RENDERER`.
- If requested backend is `vulkan` and `vk_rtx == 0`, forces external renderer load to `opengl`.

Why this location:
- `CL_InitRenderer` is where external renderer DLL selection happens.
- This guarantees true OpenGL raster/lightmap behavior instead of partial vkpt feature disablement.

### 2) Keep loaded renderer identity cvars accurate
File: `src/client/renderer.cpp`

After loading the selected backend, `r_ref` and `vid_ref` are now explicitly set to the backend actually loaded via `Cvar_SetByVar(..., FROM_CODE)`.

This prevents stale values when backend choice changes across restarts.

### 3) Preserve restart semantics for `vk_rtx`
File: `src/rend_vk/vkpt/main.c`

Updated vkpt cvar registration:
- From: `CVAR_ARCHIVE`
- To: `CVAR_ARCHIVE | CVAR_RENDERER`

Reason:
- Without this, vkpt-side `Cvar_Get` could drop `CVAR_RENDERER` on an existing `vk_rtx` cvar, breaking renderer-restart behavior when toggling `vk_rtx`.

## Behavior After Change
- `r_renderer vulkan` + `vk_rtx 1` -> Vulkan vkpt backend loads.
- `r_renderer vulkan` + `vk_rtx 0` -> OpenGL backend loads (classic-style lightmaps/raster path).
- Toggling `vk_rtx` triggers renderer restart due to `CVAR_RENDERER`.

## Validation
1. Build
   - Command: `meson compile -C builddir`
   - Result: success; rebuilt `worr.exe` and `worr_vulkan_x86_64.dll`.

2. Smoke launch (`q2dm1`)
   - Command:
     - `builddir\\worr.exe +set r_renderer vulkan +set vk_rtx 0 +set logfile 1 +set logfile_name vk_rtx_gl_fallback +set developer 1 +set deathmatch 1 +set cheats 1 +map q2dm1 +quit`
   - Result:
     - Console reports: `vk_rtx is 0, loading opengl renderer backend.`
     - OpenGL backend initialized (`GL_Init`, `Using video driver: win32wgl`).
     - `q2dm1` loaded and client entered spawned state.
     - Process exited cleanly after `+quit` with no fatal/assert recursion.
