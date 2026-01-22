# TTF Baseline Rendered Metrics Alignment (2026-01-20)

## Issue
- TTF glyphs still sat low relative to the legacy console cursor and line
  cell, even after switching away from `line_skip` scaling.

## Cause
- `TTF_GetGlyphMetrics` can report a taller glyph box than the rendered bitmap.
  SDL_ttf trims empty rows from the glyph surface, so the bitmap's top edge
  can be below `maxy`. Using `maxy` for the line baseline while positioning
  glyphs from the trimmed surface produced a consistent downward shift.
- The draw path rounded baseline and bearing separately, which could add an
  extra pixel of vertical error at small scales.

## Fix
- Compute baseline and extent using the rendered ASCII glyph surfaces by
  combining `miny` from `TTF_GetGlyphMetrics` with `surface->h` to derive the
  actual top edge for each glyph.
- Align draw Y with a single rounding step using the combined baseline offset
  (`baseline - bearing_y`) before scaling.

## Result
- TTF glyphs align with the legacy console cell height and cursor without
  extra top padding or a downward bias.
- Vertical positioning stays consistent at low virtual sizes (8px) and high
  DPI scales.
