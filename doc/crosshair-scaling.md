# Crosshair Scaling Adjustments

## Summary
Crosshair sizing now applies the integer UI scale first and then applies
`cl_crosshair_size` in pixel space. This guarantees crosshairs are scaled exactly
the same as other UI elements (including `cl_scale`), with only the final
size applied.

## Rationale
The UI scale is derived from the integer base scale and the `cl_scale` cvar.
Applying crosshair scaling after that integer scale avoids double-scaling and
keeps the crosshair size aligned with other 2D elements.

## Implementation Details
- UI scale is computed as:
  - `base_scale = floor(max(canvas_width / 640, canvas_height / 480))`
  - `ui_scale = floor(base_scale * cl_scale)` when `cl_scale` is non-zero
  - `ui_scale = base_scale` when `cl_scale` is `0`
- Crosshair pixels are computed as:
  - `pixel_size = cl_crosshair_size * ui_scale`
  - `scale = pixel_size / max(raw_width, raw_height)`
  - Drawn in pixel space with `R_SetScale(base_scale)`
- Crosshair offsets (`ch_x`, `ch_y`) are still in UI units and are converted
  using `ui_scale`.
- `cl_crosshair_size` also scales hit marker sizing via `cl_crosshair_size / 32`.

## Files
- `src/client/screen.c`
- `doc/ui-scaling-fill.md`
- `doc/client.asciidoc`
