# TTF Metrics Baseline + Kerning Realignment (2026-01-22)

## Summary
- Revert atlas packing to use the full SDL3_ttf glyph surface rather than
  trimming visible pixels.
- Keep bearings and advances sourced directly from `TTF_GetGlyphMetrics`
  so baseline placement and kerning align with FreeType metrics.

## Background
FreeType defines glyph placement using the bitmap bearings:
`bitmap_left` (horizontal bearing) and `bitmap_top` (vertical bearing). SDL_ttf
exposes those as `minx` and `maxy` in `TTF_GetGlyphMetrics` and renders glyph
surfaces sized to the full bounding box derived from those metrics. Trimming
transparent pixels shifts the bitmap relative to its bearings, which breaks
baseline alignment and makes kerning/advance spacing appear incorrect.

## Fix
- Stop trimming glyph surfaces before atlas copy.
- Preserve `bearing_x = minx`, `bearing_y = maxy`, and `advance` from
  `TTF_GetGlyphMetrics`.
- Continue using baseline placement:
  `draw_x = pen_x + bearing_x`, `draw_y = baseline_y - bearing_y`.

This restores the intended FreeType placement model and keeps kerning consistent
with the same metrics that define the glyph's bounding box.

## Files Updated
- `src/client/font.cpp`
