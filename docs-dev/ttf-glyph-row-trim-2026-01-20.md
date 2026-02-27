# TTF Glyph Row Trimming (2026-01-20)

## Issue
- TTF glyphs still looked vertically offset and undersized after switching
  to visible-pixel baselines.

## Cause
- Baselines were computed from visible rows, but glyphs were still rendered
  from surfaces that included transparent top padding. That extra padding
  shifted the visible pixels down and made them appear smaller inside the
  fixed 8px console cell.

## Fix
- Detect the first/last non-transparent rows in each glyph surface.
- Trim the copied rows into the atlas so glyphs only include visible pixels.
- Keep `bearing_y` aligned to the visible top row so baseline math matches
  the trimmed glyph data.

## Result
- TTF glyphs align to the legacy console line height without a downward bias.
- Glyphs fill the expected 8px cell height more accurately.
