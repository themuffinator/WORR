# SDL3 TTF Font Overhaul (2026-02-05)

## Summary
- Reworked SDL3_ttf metrics handling to support baseline tuning, broader glyph preloading, and configurable scale boost.
- Updated HarfBuzz rendering to shape text by color runs, preventing ligatures across color-code boundaries.
- Enabled HarfBuzz pixel snapping by default for more consistent pixel-aligned spacing.

## Rationale
The SDL3_ttf pipeline needs predictable baselines and stable spacing in a pixel-aligned UI. The proposal in
`doc/proposals/SDL3 TTF Font Handling in WORR-2.pdf` called out residual baseline drift, runtime metric
expansion when tall glyphs appear, and color-code shaping issues. This update implements the practical
parts of that plan with new tunables and more robust shaping.

## Changes
- Added `cl_font_scale_boost` (float, default `1.5`).
  - Replaces the hardcoded scale boost constant for all font types.
  - Allows testing removal of the boost by setting `cl_font_scale_boost 1`.
- Added `cl_font_ttf_baseline_offset` (int, default `0`).
  - Provides a small baseline nudge in font metric units.
  - Applied after rendered/outline metrics and clamped to the current extent.
- Added `cl_font_ttf_preload_max` (int, default `255`, clamp `126..2048`).
  - Expands preload from ASCII to Basic Latin-1 (or higher if desired) to keep
    rendered baseline metrics stable when extended glyphs appear.
- Changed `cl_font_ttf_hb_snap` default to `1`.
  - HarfBuzz glyph positions now snap by default for consistent pixel alignment.
- HarfBuzz shaping now splits on color runs.
  - Color codes become explicit shaping breaks so ligatures do not span color changes.
  - `Font_DrawString` and `Font_MeasureString` share the same run segmentation to
    keep measured widths consistent with rendered output.

## Implementation Notes
- `font_preload_ttf_glyphs()` replaces ASCII-only preloading and uses
  `cl_font_ttf_preload_max` to determine the range.
- `font_build_shaped_text()` now records color runs; rendering and measurement
  iterate runs per line instead of shaping the whole line at once.
- Debug output now includes baseline offset and scale boost to make tuning easier.

## Validation Checklist
- `cl_font_debug_draw 1` and `cl_font_debug_draw 16` for baseline/extent overlays.
- `cl_font_debug_dump 1` on a sample string to verify metrics and run shaping.
- Test color-boundary shaping:
  - Example: `^1fi^7` should not form an `fi` ligature across the color change.
- Test extended glyphs to ensure no runtime baseline shift:
  - Example: `ÅÁÉ` should not alter previously rendered baselines.
- Test snap vs. unsnap:
  - `cl_font_ttf_hb_snap 0/1` to verify spacing consistency.

## Files Touched
- `src/client/font.cpp`
