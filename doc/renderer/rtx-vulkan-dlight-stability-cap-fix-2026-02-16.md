# RTX/Vulkan Dlight Stability and Cap-Pressure Fixes (2026-02-16)

## Goal
Resolve persistent shadowlight/dynamic-light instability on active non-GL renderers by:
- fixing RTX dynamic-light upload safety,
- improving dlight prioritization under hard caps,
- stabilizing light selection across frames,
- exposing cap pressure in HUD/debug cvars for Vulkan/RTX and shared client dlight assembly.

## Root Causes Identified

### 1) RTX dynamic-light uniform overflow risk
- `src/rend_rtx/vkpt/main.c` uploaded `num_lights` directly into `ubo->dyn_light_data`.
- The runtime dlight pool can contain up to `MAX_DLIGHTS` (`64`), but RTX shader storage defines `MAX_LIGHT_SOURCES` (`32`).
- This created an overrun hazard and undefined uniform contents under heavy light counts.

### 2) Incomplete `dlight_t` initialization feeding RTX
- `src/client/view.cpp` did not fully initialize all per-light fields on reuse/replacement paths.
- `light_type`/spot payload could remain stale in reused slots because `r_dlights[]` was not fully cleared each frame.
- RTX packing in `add_dlights()` relies on `light_type` and spot payload to build `DynLightData`.

### 3) Shared dlight cap pressure causing visible popping
- The shared client light list still enforces hard cap competition (`MAX_DLIGHTS=64`) with no temporal stickiness.
- Shadowlights and strict-PVS shadowlights could lose slots to transient lights during camera motion.

## Implemented Fixes

### A) Shared client dlight stabilization (`src/client/view.cpp`)
- Added weighted scoring controls:
  - `cl_dlight_shadowlight_priority` (default `1.15`)
  - `cl_dlight_shadowlight_strict_pvs_priority` (default `1.35`)
  - `cl_dlight_sticky_ms` (default `250`)
  - `cl_dlight_sticky_boost` (default `1.25`)
- Added sticky light-key cache for temporal selection stability.
- Added strict-PVS-aware shadowlight insertion path:
  - New API: `V_AddLightExVis(cl_shadow_light_t *light, bool strict_pvs)`
  - Existing `V_AddLightEx` now wraps with `strict_pvs=true`.
- Fully initialize selected dlight slots (`memset`) before assignment.
- Explicitly set RTX-relevant fields:
  - `dl->light_type` (`DLIGHT_SPHERE`/`DLIGHT_SPOT`)
  - spot emission profile and spot payload for cone lights.
- Mark static shadowlights as `DL_SHADOW_LIGHT` when shadowlights are enabled, independent of renderer per-pixel-lighting support.

### B) Strict-PVS signal from shadowlight source (`src/client/effects.cpp`)
- `CL_AddShadowLights()` now computes:
  - `strict_pvs = (ent->serverframe == cl.frame.number)`
- Calls `V_AddLightExVis(..., strict_pvs)` so shared scoring can prioritize strict-PVS shadowlights over fallback baseline lights.

### C) RTX-safe capped upload and scoring (`src/rend_rtx/vkpt/main.c`)
- Reworked `add_dlights()`:
  - hard caps uploads to `MAX_LIGHT_SOURCES`,
  - selects best candidates by score instead of blind first-N upload,
  - adds sticky temporal boost and static-shadowlight priority.
- Added defensive early-return handling for null/empty light lists.
- Added deterministic pick sorting to reduce per-frame light reorder noise.
- Added debug/stat cvars:
  - `rtx_draw_debug`
  - `rtx_dlight_shadowlight_priority`
  - `rtx_dlight_sticky_ms`
  - `rtx_dlight_sticky_boost`
  - `rtx_debug_dlights_total`
  - `rtx_debug_dlights_uploaded`
  - `rtx_debug_dlights_cap`
  - `rtx_debug_dlights_dropped`
  - `rtx_debug_shadowlights_total`
  - `rtx_debug_shadowlights_uploaded`

### D) HUD debug integration (`src/game/cgame/cg_draw.cpp`)
- Extended FPS debug line to support three modes:
  - existing GL shadow debug (`gl_shadow_draw_debug`),
  - RTX dlight-cap debug (`rtx_draw_debug`),
  - shared client dlight debug (`cl_dlight_draw_debug`).
- Registered and displayed new stats for RTX and shared client dlight pipeline:
  - `cl_dlight_debug_requested`
  - `cl_dlight_debug_selected`
  - `cl_dlight_debug_cap`
  - `cl_dlight_debug_dropped`
  - `cl_dlight_debug_shadow_requested`
  - `cl_dlight_debug_shadow_strict_requested`

## File List
- `src/client/client.h`
- `src/client/effects.cpp`
- `src/client/view.cpp`
- `src/rend_rtx/vkpt/main.c`
- `src/game/cgame/cg_draw.cpp`

## Validation
- Build:
  - `meson compile -C builddir` succeeded.
- Smoke runs:
  - `worr.exe` with `+set r_renderer rtx ... +map q2dm1 +quit`
  - `worr.exe` with `+set r_renderer vulkan ... +map q2dm1 +quit`
  - both runs completed without renderer crash/regression.
