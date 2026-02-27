# TTF HB Metrics Alignment + Debug Tools (2026-01-27)

## Summary
- HarfBuzz glyph-index caching now prefers SDL_ttf bitmap metrics when a glyph is
  nominally mapped to a single codepoint. This aligns bearings/baselines with
  the actual rasterized bitmap instead of outline-only extents.
- Rendered-metrics tracking now skips glyphs with invalid metrics and updates
  incrementally as new glyphs are cached, keeping baseline/extent aligned with
  real glyph bounds.
- New debug toggles expose detailed metrics comparisons (TTF vs HB) and outline
  metric guides to pinpoint baseline/descender and spacing issues.

## Behavior changes
- HB glyph caches can carry TTF-derived bearings/advances for nominal glyphs,
  improving descender and baseline stability without disabling shaping.
- Rendered metrics can grow when new glyphs extend beyond the initial ASCII
  preload; fixed-advance scaling is updated when this happens.

## New debug cvars
### `cl_font_debug_metrics` (bool)
Enables extra per-glyph metric dumps when `cl_font_debug_dump` is triggered.
Includes TTF metrics (min/max/advance) and HB extents/positions for the glyph.

### `cl_font_debug_draw` (bitmask)
Adds a new flag:
- `16` = outline metric guides (purple/orange lines) drawn relative to the
  current baseline for comparison against rendered metrics.

Example:
- `set cl_font_debug_draw 23` (lines + bounds + advances + outline guides)

### `cl_font_ttf_hb_snap` (bool, default 0)
Optional pixel snapping for HarfBuzz positions/offsets in the HB draw/measure
paths. Useful to test whether spacing issues are caused by fractional advances.

## Debug workflow
1) Filter to a string: `set cl_font_debug_match "gjpqy"`
2) Enable overlays: `set cl_font_debug_draw 23`
3) Trigger dump: `set cl_font_debug_dump 1`
4) If spacing still looks off, toggle `set cl_font_ttf_hb_snap 1` and compare
   output while keeping `cl_font_debug_metrics 1`.

## Files
- `src/client/font.cpp`
