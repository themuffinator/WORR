# TTF Rendered Metrics Freeze (2026-02-05)

## Summary
- Rendered TTF metrics are now **frozen after initial preload** to prevent
  per-glyph updates from inflating line height and shrinking advances at
  runtime.

## Root Cause
For proportional fonts (HarfBuzz path), glyph metrics were still being updated
on-demand as new glyphs were cached. Because SDL3_ttf surfaces include full
line-height padding, these late updates could **grow the rendered extent**, which
in turn reduced `glyph_scale` and made horizontal spacing too tight. The effect
showed up in font dumps as `adv_px` values much smaller than expected and
visually as excessive line padding.

## Fix
- Added a `rendered_metrics_locked` flag on `font_ttf_t`.
- After ASCII preload and `font_compute_ttf_rendered_metrics`, lock the rendered
  metrics.
- Skip incremental metric updates when the lock is active.

This keeps baseline/extent stable for the lifetime of the font and avoids
spacing changes as glyphs are streamed in.

## Validation
1. Rebuild and run `font_dump_glyphs` at `line_height=32`.
2. Confirm `adv_px` matches `adv * glyph_scale` (no mid-dump scale drift).
3. In-game, verify UI/scr fonts no longer compress horizontally and line
   padding is closer to expected values.

## Files Updated
- `src/client/font.cpp`
