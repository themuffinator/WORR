# CGame HUD Shared Scaling

## Summary
CGame HUD rendering now uses the same virtual scaling path as all other 2D UI
elements. The previous 320x240 virtual override has been removed so cgame
coordinates map directly into the shared HUD canvas.

## Virtual Screen
- HUD vrect now matches the shared HUD canvas:
  - `hud_rect.width  = scr.hud_width`
  - `hud_rect.height = scr.hud_height`
- Safe zone uses the same fractional scaling as other 2D elements.

## Draw Call Mapping
- Cgame draw wrappers no longer apply extra scaling.
- Font string alignment is computed in shared HUD units and drawn directly.
- The render pass still uses `R_SetScale(scr.hud_scale)` like other 2D elements.

## Files
- `src/client/screen.c`
- `src/client/cgame.c`
