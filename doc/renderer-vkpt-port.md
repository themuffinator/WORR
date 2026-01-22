# Vulkan RTX Renderer Port (vkpt)

## Goal
Replace the preliminary Vulkan renderer with the full Q2RTX vkpt renderer while keeping WORR engine/ABI compatibility.

## Renderer API alignment
- Converted vkpt entry points to the shared renderer API signatures (no `_RTX` indirection).
- Added direct `R_IsHDR`, `R_SupportsDebugLines`, `R_AddDebugText_`, `R_VideoSync`, `R_SupportsPerPixelLighting`, and `R_GetGLConfig` exports to match `renderer_api.c` expectations.
- Wired refresh function pointers (`IMG_*`, `MOD_*`) in `R_Init` and added `refresh_ptrs.c` to own those globals plus `r_config`/`registration_sequence`.

## UI/HUD and font handling
- Replaced missing `SCR_*` helpers in vkpt with local alignment + `R_DrawStringStretch` calls.
- Added `vkpt_draw_set_hud_alpha` so vkpt can fade the HUD during accumulation/photo mode.
- Implemented full `R_DrawChar`, `R_DrawStretchChar`, `R_DrawStringStretch`, `R_DrawKFontChar`, `SCR_KFontLookup`, and `SCR_LoadKFont` for vkpt.
- Implemented `R_DrawStretchRotatePic` rotation support in the vkpt stretch-pic path.

## Freecam input
- Removed reliance on client globals and SDL input.
- Pulled mouse/keyboard cvars via `Cvar_Get` and recomputed autosens locally (matches client logic).
- Switched to native `vid->get_mouse_motion` and vkpt refdef FOV for freecam sensitivity.

## Physical sky cleanup
- Removed SDL gamepad initialization and input hooks.
- Replaced `cl.maxclients` checks with `Cvar_VariableInteger("maxclients")`.

## Refresh integration
- `inc/refresh/images.h` now exposes both GL and vkpt image payload fields to keep ABI/layout stable when both REF macros are defined.
- `refresh/models.c` no longer depends on client/GL headers; test model placement uses `vkpt_refdef`.

## Build updates
- `renderer_vk_src` now builds vkpt + refresh sources (plus `refresh/stb/stb.c`) instead of `vk_main.c`.
- Added vkpt include paths for `refresh/stb` and `third_party`.
- Defined `REF_GL=1` and `REF_VKPT=2` globally; set `USE_REF` per renderer target (`REF_GL` for GL, `REF_VKPT` for vkpt).

## Known limitations
- None tracked here after the stretch-rotate fix; add new entries as they are discovered.
