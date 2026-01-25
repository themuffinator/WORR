# Cvar Snake-Case Renames (2026-01-25)

## Goal
Normalize mixed-case cvar names to lowercase snake_case while keeping legacy
configs working.

## Compatibility behavior
- Each renamed cvar keeps a legacy alias using the old camelCase name.
- Legacy aliases use `CVAR_NOARCHIVE` so new configs only persist the snake_case names.
- On startup, if either alias was modified, values are synchronized so both names stay in lockstep.

## Renamed cvars

### Crosshair (client)
- `cl_crosshairBrightness` -> `cl_crosshair_brightness`
- `cl_crosshairColor` -> `cl_crosshair_color`
- `cl_crosshairHealth` -> `cl_crosshair_health`
- `cl_crosshairHitColor` -> `cl_crosshair_hit_color`
- `cl_crosshairHitStyle` -> `cl_crosshair_hit_style`
- `cl_crosshairHitTime` -> `cl_crosshair_hit_time`
- `cl_crosshairPulse` -> `cl_crosshair_pulse`
- `cl_crosshairSize` -> `cl_crosshair_size`

### Client/UI
- `cl_weaponBar` -> `cl_weapon_bar`
- `cl_debugFonts` -> `cl_debug_fonts`
- `cl_fontGlyphCacheSize` -> `cl_font_glyph_cache_size`

### Cgame HUD
- `cg_drawFPS` -> `cg_draw_fps`
- `cl_skipHud` -> `cg_skip_hud`
- `cl_hud_cgame` -> `cg_hud_cgame`
- `cl_obituary_time` -> `cg_obituary_time`
- `cl_obituary_fade` -> `cg_obituary_fade`

### Renderer (screenshots)
- `gl_screenshot_format` -> `r_screenshot_format`
- `gl_screenshot_quality` -> `r_screenshot_quality`
- `gl_screenshot_compression` -> `r_screenshot_compression`
- `gl_screenshot_async` -> `r_screenshot_async`
- `gl_screenshot_message` -> `r_screenshot_message`
- `gl_screenshot_template` -> `r_screenshot_template`

### Renderer (OpenGL shared)
- `r_overBrightBits` -> `r_overbright_bits`
- `r_mapOverBrightBits` -> `r_map_overbright_bits`
- `r_mapOverBrightCap` -> `r_map_overbright_cap`
- `r_dofBlurRange` -> `r_dof_blur_range`
- `r_dofFocusDistance` -> `r_dof_focus_distance`
- `r_crt_hardPix` -> `r_crt_hard_pix`
- `r_crt_hardScan` -> `r_crt_hard_scan`
- `r_crt_maskDark` -> `r_crt_mask_dark`
- `r_crt_maskLight` -> `r_crt_mask_light`
- `r_crt_scaleInLinearGamma` -> `r_crt_scale_in_linear_gamma`
- `r_crt_shadowMask` -> `r_crt_shadow_mask`
- `r_picmipFilter` -> `r_picmip_filter`

## Notes for configs and scripts
- Prefer the new names in configs, UI bindings, and scripts.
- Legacy names remain accepted but are not written back to config files.
