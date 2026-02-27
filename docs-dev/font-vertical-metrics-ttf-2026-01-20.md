# TTF Vertical Metrics Alignment (2026-01-20)

## Summary
Adjusted TTF vertical metric handling to use SDL3_ttf typographic metrics
consistently for baseline and extent. This fixes per-glyph vertical placement
by keeping the baseline and glyph bearings in the same metric space.

## SDL3_ttf References
- `TTF_GetFontAscent`: positive offset from baseline to top.
- `TTF_GetFontDescent`: negative offset from baseline to bottom.
- `TTF_GetGlyphMetrics`: per-glyph `maxy`/`miny` relative to baseline.

## Changes
- `font_compute_ttf_metrics` now computes:
  - `baseline` from ascent
  - `extent` from `ascent + abs(descent)`
- Removed the raster-scan path for computing ASCII min/max rows, which mixed
  pixel-scanned extents with typographic bearings.

## Rationale
SDL3_ttf defines ascent/descent and glyph bearings relative to the baseline.
Using those metrics uniformly keeps the `baseline - bearing_y` positioning
correct at any scale, and avoids vertical drift caused by mixed metric sources.

## Files Touched
- `src/client/font.cpp`
