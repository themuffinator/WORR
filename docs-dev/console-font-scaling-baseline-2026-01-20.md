# Console Font Scaling + Baseline Rounding

**Date:** 2026-01-20  
**Scope:** `src/client/console.cpp`, `src/client/font.cpp`

## Summary
- Reduced console font size and input line height by halving the virtual
  line height and fixed advance used for console font loading.
- Stabilized TTF glyph placement by rounding the baseline position once
  per glyph draw, reducing rounding drift in vertical alignment.

## Changes
### Console font load sizing
`Con_RegisterMedia` now computes:
- `con_line_height = max(1, CONCHAR_HEIGHT / 2)`
- `con_fixed_advance = max(1, CONCHAR_WIDTH / 2)`

These values are used for both the configured console font and its fallback,
so console text and the input line scale down consistently.

### TTF glyph draw rounding
`font_draw_ttf_glyph_cached` now:
- Computes `baseline_y` as a float.
- Rounds the final `draw_y` once after applying the glyph bearing.

This keeps baseline placement consistent while reducing per-term rounding
error for glyphs with small vertical offsets.

## Expected Results
- Console text renders at ~50% of the previous size.
- Input line height and separator spacing shrink proportionally.
- Baseline alignment remains stable with less rounding jitter.

## Verification
1. Launch WORR and open the console.
2. Compare console font size and input line height against the previous build.
3. Confirm glyph baselines align consistently across lines.
