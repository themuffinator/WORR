# 2D Virtual Screen Scaling (640x480)

## Overview
2D rendering now uses a virtual 640x480 reference space, uniformly scaled to
fill the current canvas. This keeps HUD/UI sizing consistent across aspect
ratios while preserving a single, predictable coordinate system for all 2D
draws.

## Virtual Screen Math
- Reference size: `VIRTUAL_SCREEN_WIDTH = 640`, `VIRTUAL_SCREEN_HEIGHT = 480`.
- Base scale (pixels per virtual unit):
  - `s = max(canvas_width / 640, canvas_height / 480)`
  - `s` is floored to an integer to keep UI scaling pixel-aligned.
- Virtual canvas size (virtual units that map to the full canvas):
  - `virtual_width  = canvas_width / s`
  - `virtual_height = canvas_height / s`
- Pixel conversion for any 2D coordinate:
  - `pixels_per_unit = s / draw_scale`

## Renderer Changes
- `GL_Setup2D` updates `draw.base_scale`, `draw.virtual_width`,
  `draw.virtual_height`, and uses a full-canvas viewport with:
  - `GL_Ortho(0, virtual_width, virtual_height, 0, -1, 1)`
- `R_SetScale` applies extra 2D scaling by expanding/shrinking the virtual
  ortho bounds:
  - `GL_Ortho(0, virtual_width * scale, virtual_height * scale, 0, -1, 1)`
- `R_SetClipRect` converts from virtual units to pixels using
  `draw.base_scale / draw.scale`.
- Postprocess and blend overlays convert the pixel-space refdef rect to
  virtual units after `GL_Setup2D` to keep full-screen quads aligned.

## Client Changes
- `scr.canvas_width/height` store real pixel dimensions for 3D view sizing.
- `scr.virtual_scale` and `scr.virtual_width/height` drive all 2D placement.
- `SCR_CalcVrect` uses `scr.canvas_*` (3D view remains in pixel units).
- `SCR_TileClear` converts the pixel `scr.vrect` to virtual units before
  drawing.
- Cinematics and loading screens size/center using virtual units.
- HUD and UI use the same virtual scaling path.

## UI Input Mapping
- Mouse input arrives in pixel coordinates and is converted to virtual units:
  - `virtual = pixel / scr.virtual_scale`
  - `ui = virtual * uis.scale`
- `IN_WarpMouse` uses the inverse mapping to keep cursor warping aligned.
- `uis.canvas_width/height` preserves resolution-based layout decisions.

## Scaling CVars
- `scr_scale`, `ui_scale`, and `con_scale` multiply the base UI scale.
- The resulting UI scale is floored to an integer to avoid fractional sizing.
- Auto scale (`0`) keeps the base UI scale with no extra multiplier.
