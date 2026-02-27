# TTF Glyph Padding Offset Fix (2026-02-05)

## Summary
- Reverted the aggressive glyph trimming approach.
- Added padding measurement on SDL3_ttf glyph surfaces and apply **offsets**
  without altering atlas sizes.
- Aligns baseline to actual glyph pixels while keeping atlas packing stable.

## Root Cause
SDL3_ttf glyph surfaces can include transparent padding above/below the actual
pixel coverage. We were using the full surface height to compute `offset_y`,
which effectively added **bottom padding** into the baseline offset and pushed
most glyphs lower than the debug baseline guide.

## Fix
- Scan the alpha channel of the surface to find top/left padding.
- Keep `glyph.w/h` as the full surface size (no trimming), but set:
  - `glyph.offset_x = -pad_left`
  - `glyph.offset_y = pad_top`
- Apply these offsets for both codepoint and HarfBuzz glyph-index paths.

This preserves atlas reuse and avoids the regression caused by trimming while
correcting baseline placement for padded surfaces.

## Validation
- Use `cl_font_debug_draw 27` to verify the green baseline intersects
  non‑descending glyphs and descenders sit between the green and red guides.
- Compare before/after using `AtkinsonHyperLegible-Regular.otf` in console/UI.

## Files Updated
- `src/client/font.cpp`
