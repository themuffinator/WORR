# Font Scaling + Bearing Alignment Fixes (2026-01-20)

## Summary
This update implements the remaining recommendations from
`docs-dev/proposals/font-implementation-analysis-2026-01-20.md` for font sizing
and vertical alignment.

## Changes
- **Unit scale now accounts for DPI/pixel scale** for all font kinds
  (TTF/OTF, kfont, legacy). The scale is computed using
  `virtual_line_height * pixel_scale` so glyphs render at the correct
  size at higher resolutions.
- **TTF baseline now uses typographic ascent** instead of a raster-scan
  maximum. This keeps the baseline consistent with typographic bearings.
- **TTF glyph bearings are kept typographic** (from `TTF_GetGlyphMetrics`)
  and are no longer overridden by raster-scan values during glyph upload.
  This ensures the `baseline - bearing` math matches the font metrics.
- **Per-glyph raster row trimming is disabled** in the glyph upload path
  to avoid mismatches between trimmed bitmaps and typographic bearings.

## Rationale
The previous scaling math ignored `pixel_scale`, which made text too small
at higher DPI. Additionally, glyph bearings were sometimes taken from
raster scans while the baseline was derived differently, causing vertical
misalignment. The new approach uses typographic metrics for alignment and
applies pixel-correct scaling uniformly.

## Files Touched
- `src/client/font.cpp`
