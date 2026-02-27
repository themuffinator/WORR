# Cvar Prefix Renames (2026-01-25)

## Summary
- Standardized renderer and screen cvar prefixes, replacing legacy `vid_` and `scr_` names with subsystem-specific prefixes.
- Preserved legacy names as `CVAR_NOARCHIVE` aliases to keep existing configs and scripts working.
- Updated UI/menu references and documentation to use the new primary names.

## Rename Map

### Renderer and Video
- `vid_fullscreen` -> `r_fullscreen`
- `vid_fullscreen_exclusive` -> `r_fullscreen_exclusive`
- `vid_display` -> `r_display`
- `vid_geometry` -> `r_geometry`
- `vid_modelist` -> `r_modelist`
- `vid_driver` -> `r_driver`
- `vid_gamma` -> `r_gamma` (OpenGL)
- `vid_hwgamma` -> `r_hwgamma`
- `vid_flip_on_switch` -> `win_flip_on_switch` (Windows-only)
- `vid_vsync` -> `vk_vsync` (vkpt)
- `vid_hdr` -> `vk_hdr` (vkpt)

### Client Screen and HUD
- `scr_draw2d` -> `cl_draw2d`
- `scr_showpause` -> `cl_showpause`
- `scr_showturtle` -> `cl_showturtle`
- `scr_demobar` -> `cl_demobar`
- `scr_scale` -> `cl_scale`
- `scr_alpha` -> `cl_alpha`
- `scr_font` -> `cl_font`
- `scr_chathud` -> `cl_chathud`
- `scr_chathud_lines` -> `cl_chathud_lines`
- `scr_chathud_time` -> `cl_chathud_time`
- `scr_chathud_x` -> `cl_chathud_x`
- `scr_chathud_y` -> `cl_chathud_y`
- `scr_lag_draw` -> `cl_lag_draw`
- `scr_lag_x` -> `cl_lag_x`
- `scr_lag_y` -> `cl_lag_y`
- `scr_lag_min` -> `cl_lag_min`
- `scr_lag_max` -> `cl_lag_max`
- `scr_showstats` -> `cl_showstats`
- `scr_showpmove` -> `cl_showpmove`
- `scr_hit_marker_time` -> `cl_hit_marker_time`
- `scr_damage_indicators` -> `cl_damage_indicators`
- `scr_damage_indicator_time` -> `cl_damage_indicator_time`
- `scr_pois` -> `cl_pois`
- `scr_poi_edge_frac` -> `cl_poi_edge_frac`
- `scr_poi_max_scale` -> `cl_poi_max_scale`
- `scr_safe_zone` -> `cl_safe_zone`

### Client Game
- `scr_centertime` -> `cg_centertime`
- `scr_printspeed` -> `cg_printspeed`
- `scr_maxlines` -> `cg_maxlines`
- `scr_usekfont` -> `cg_usekfont`

### Console
- `scr_conspeed` -> `con_speed`

## Alias Behavior
- Every renamed cvar registers a legacy alias with `CVAR_NOARCHIVE` (and `CVAR_RENDERER` where relevant).
- Aliases are bidirectionally synced via `changed` callbacks and default value sync on init.
- `vid_ref` remains read-only for compatibility, and the `vid_restart` command name is unchanged.

## Implementation Notes
- Renderer cvar aliases live in `src/client/renderer.cpp` with sync helpers for `r_*` vs. `vid_*`.
- OpenGL gamma uses `r_gamma` with a `vid_gamma` alias in `src/rend_gl/texture.c`.
- SDL and Win32 backends alias `r_hwgamma` and `win_flip_on_switch` in `src/unix/video/sdl.c` and `src/windows/client.c`.
- vkpt registers `vk_vsync`/`vk_hdr` with `vid_*` aliases in `src/rend_vk/vkpt/main.c`.
- Screen/HUD aliases are centralized in `src/client/screen.cpp` for `cl_*` replacements.
- Console speed uses `con_speed` with the `scr_conspeed` alias in `src/client/console.cpp`.
- Cgame HUD/centerprint cvars use `cg_*` names with `scr_*` aliases in `src/game/cgame/cg_draw.cpp` and `src/client/cgame_classic.cpp`.
- UI menu bindings were updated in `src/client/ui/worr.menu` and `src/game/cgame/ui/worr.json`.

## Compatibility
- Legacy configs and scripts continue to work via aliases.
- No protocol or demo format changes.

## Testing
- Not run (config/cvar rename change set).
