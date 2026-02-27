# Vulkan Menu-Mode Parity + `vk_rtx` Toggle (2026-02-10)

## Goal
1. Stop Vulkan from entering menu post-process mode on map start unless the UI menu blur path is actually active (parity with GL behavior).
2. Add a Vulkan-facing RTX toggle cvar: `vk_rtx`.

## Root Cause
In `src/rend_vk/vkpt/main.c`, menu-mode was driven by:
- `qvk.frame_menu_mode = cl_paused->integer == 1 && render_world;`

`cl_paused == 1` is set by autopause when either console **or** menu is up, so Vulkan could enter menu-mode during startup/console states where GL does not apply equivalent menu-only post-processing.

## Changes
### 1) Menu-mode parity fix
File: `src/rend_vk/vkpt/main.c`

Replaced broad pause-based gate with a menu-blur-frame gate derived from refdef:
- `render_world`
- `fd->dof_rect_enabled`
- valid `dof_rect`
- `fd->dof_strength > 0`

This narrows `qvk.frame_menu_mode` to actual menu blur frames, avoiding false positives at level start.

### 2) Added `vk_rtx` cvar
File: `src/rend_vk/vkpt/main.c`

Added:
- `cvar_t *cvar_vk_rtx`
- registration: `Cvar_Get("vk_rtx", "1", CVAR_ARCHIVE)`
- change callback to reset temporal/accumulation state when toggled.

Behavior implemented in `evaluate_reference_mode`:
- If `vk_rtx == 0`:
  - disable accumulation
  - disable denoiser
  - force `num_bounce_rays = 0`
  - force `reflect_refract = 0`
  - reset HUD alpha override to normal

This provides a single Vulkan cvar to switch to a lighter, less RTX-heavy visual profile that is closer to GL’s look.

## Launch config update
File: `.vscode/launch.json`

Vulkan launch args now use:
- `+set vk_rtx 0`

and no longer rely on manual `pt_num_bounce_rays`/`pt_reflect_refract` launch overrides.

## Validation
1. Build:
- `meson compile -C builddir`
- Result: success (`worr_vulkan_x86_64.dll` rebuilt and linked).

2. Runtime smoke test:
- `builddir\\worr.exe +set r_renderer vulkan +set vk_rtx 0 +set logfile 1 +set logfile_name vk_rtx0_q2dm1 +set developer 1 +set deathmatch 1 +set cheats 1 +map q2dm1 +quit`
- Result:
  - Vulkan initialized
  - `q2dm1` loaded
  - no assertion/fatal/recursive `Z_Free` error in log scan

Log file:
- `C:\Users\djdac\Saved Games\Nightdive Studios\Quake II\baseq2\logs\vk_rtx0_q2dm1.log`

## Notes
- `vk_rtx` is a runtime quality/feature toggle for vkpt behavior, not a backend swap; the Vulkan renderer still uses vkpt infrastructure.
- Existing unrelated local edits were left untouched.
