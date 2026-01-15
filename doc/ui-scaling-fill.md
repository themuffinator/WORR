# UI Scaling Fill Mode

## Summary
UI scaling now uses a fill-based integer scale that targets the larger canvas
axis. This produces a 4x UI scale on a 2560x1440 canvas (24x24 becomes 96x96),
while keeping all HUD and UI elements on the same virtual scaling path.

## Base Scale
- Reference size: `VIRTUAL_SCREEN_WIDTH = 640`, `VIRTUAL_SCREEN_HEIGHT = 480`.
- Base scale (integer):
  - `base_scale = floor(max(canvas_width / 640, canvas_height / 480))`
  - `base_scale` is clamped to a minimum of 1.
- Virtual dimensions:
  - `virtual_width  = canvas_width / base_scale`
  - `virtual_height = canvas_height / base_scale`

## Additional Scaling (`scr_scale`)
- `scr_scale` multiplies the base scale:
  - `ui_scale = floor(base_scale * scr_scale)` when `scr_scale` is non-zero.
  - `ui_scale = base_scale` when `scr_scale` is `0`.
- The renderer uses:
  - `R_SetScale(base_scale / ui_scale)`
  - This keeps pixels-per-unit equal to `ui_scale`.

## Crosshair Scaling
- `cl_crosshairSize` sets a pixel size target for crosshair sizing.
- Default: `cl_crosshairSize = 32`.
- Crosshair size applies `cl_crosshairSize` after the integer UI scale is
  calculated, using pixel-space sizing to avoid double-scaling.
- `cl_crosshairSize` also scales hit marker sizing via `cl_crosshairSize / 32`.

## Files
- `src/rend_gl/draw.c`
- `src/rend_gl/state.c`
- `src/client/screen.c`
- `src/client/ui/ui.c`
