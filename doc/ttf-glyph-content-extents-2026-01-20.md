# TTF Glyph Content Extents (2026-01-20)

## Issue
- TTF text remained vertically offset and appeared smaller than the legacy
  console font, especially after switching to rendered-surface baselines.

## Cause
- SDL_ttf glyph metrics and surfaces can include extra transparent padding
  above or below the visible pixels.
- Using those padded bounds for the baseline/extent shrinks the scale and
  shifts glyphs vertically, even though the drawn pixels occupy less space.

## Fix
- Scan each ASCII glyph surface for the first/last rows with alpha to find the
  true visible top/bottom.
- Use the visible rows to compute the global baseline and extent, which keeps
  the scale tight to the actual glyph pixels.
- Set `bearing_y` from the visible top row so glyphs align to the computed
  baseline without extra padding.

## Result
- TTF glyphs align with the console line height and legacy cursor.
- Visual size matches expectations while preserving DPI clarity.
