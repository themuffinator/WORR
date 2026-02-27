# TTF Debug Tools + Rendered Metrics (2026-01-27)

## Summary
- Added per-string on-screen debug overlays for TTF glyph placement and line metrics.
- Added a one-shot metric dump to the console for the next matching string.
- Added rendered-metrics mode to align baseline/extent to the actual rasterized
  glyph bounds (ASCII preload), improving baseline stability and spacing.

## New cvars
### `cl_font_debug_draw` (bitmask)
Draw on-screen debug overlays for TTF text.
- `1` = line guides (top, baseline, bottom)
- `2` = glyph bitmap bounds
- `4` = advance markers (pen X)
- `8` = origin points (pen X, baseline)

Example:
- `set cl_font_debug_draw 7` (lines + bounds + advances)

### `cl_font_debug_dump` (one-shot)
When set to `1`, prints metrics for the next matching TTF string and resets to `0`.

### `cl_font_debug_match` (string filter)
Only draw/print debug data when the drawn string contains this substring.
Default empty string = no filtering.

Example:
- `set cl_font_debug_match "Score"`
- `set cl_font_debug_dump 1`

### `cl_font_ttf_metric_mode`
Controls which font metrics are used for baseline + scaling.
- `0` = outline metrics (SDL_ttf ascent/extent)
- `1` = rendered metrics (ASCII preload, uses `bearing + offset` + bitmap height)

Default is `1` for improved baseline alignment with rendered glyphs.

## Implementation notes
- Rendered metrics are computed after ASCII preload and stored per font:
  `rendered_ascent`, `rendered_descent`, `rendered_extent`.
- Metric scale now uses `rendered_extent` when enabled, keeping advances and
  glyph sizes consistent with the same basis.
- Fixed-advance spacing respects the selected metric mode for TTF fonts.

## Files
- `src/client/font.cpp`

## Quick workflow
1) `set cl_font_debug_match "g"`
2) `set cl_font_debug_draw 7`
3) `set cl_font_debug_dump 1`
4) Open console or UI and look for glyph boxes vs baseline/line guides.
