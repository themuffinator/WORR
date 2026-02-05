# TTF Rendered Metrics Exclude Bitmap Padding (2026-02-05)

## Summary
- Rendered TTF metrics now ignore transparent bitmap padding when computing
  `rendered_ascent`, `rendered_descent`, and `rendered_extent`.
- Stores top/bottom padding per glyph and derives *trimmed* bounds from
  non‑transparent pixels.

## Root Cause
SDL3_ttf glyph surfaces often include empty rows above/below the visible glyph.
We were computing rendered metrics from the *full* surface bounds:
`top = bearing_y + offset_y`, `bottom = top - h`. Because `offset_y` already
compensates for top padding, this effectively **counted top and bottom padding
as real glyph extents**. The inflated `rendered_extent` reduced the metric scale,
making proportional fonts appear too small and causing tight horizontal spacing.

This is visible in font dumps where `dT`/`dB` show significant padding while
`rendered_extent` still matches the full surface height.

## Fix
- Track `pad_top` and `pad_bottom` on each glyph (codepoint and HB index).
- When computing rendered metrics, use the *trimmed* height:
  `trimmed_h = h - pad_top - pad_bottom` and
  `top = bearing_y + offset_y - pad_top`.
- This bases `rendered_ascent/descent` on actual pixel bounds rather than
  empty padding rows.

## Validation
1. Run `font_dump_glyphs` with `line_height=32` and confirm:
   - `rendered_extent` is closer to the *visible* glyph height, not `h`.
   - `adv_px` values increase to match expected spacing for proportional fonts.
2. Use `cl_font_debug_draw 27` and verify:
   - Green baseline intersects non‑descenders.
   - Advances no longer compress proportional text (UI/scr fonts).

## Files Updated
- `src/client/font.cpp`
