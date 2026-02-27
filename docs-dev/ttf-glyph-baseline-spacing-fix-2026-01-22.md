# TTF Baseline + Horizontal Spacing Fix (2026-01-22)

## Summary
- Restore baseline-based vertical placement for TTF glyphs instead of
  top-aligning them to the line box.
- Trim transparent columns from SDL3_ttf glyph surfaces and update
  `bearing_x` to match the visible left edge, keeping advances intact.

## Issue
- After trimming only rows and using `max(ascent, bearing_y)` for placement,
  most glyphs ended up with their top at the line origin, making short
  characters appear top-aligned with taller ones.
- SDL3_ttf glyph surfaces can include empty columns on the left/right, so
  drawing with the typographic bearings could still show uneven spacing.

## Fix
- Replace the baseline offset clamp with direct baseline math:
  `draw_y = baseline_y - bearing_y * scale`.
- Scan glyph surfaces for the full non-transparent bounds and trim both rows
  and columns.
- Adjust `bearing_x` by the trimmed left offset and `bearing_y` by the trimmed
  top offset to keep baseline math in the same coordinate space as the stored
  pixels.

## Files Updated
- `src/client/font.cpp`
