# SDL3_ttf Scaling/Placement Fixes (2026-01-27)

## Summary
- Centralized TTF/HarfBuzz unit-to-screen scaling with a single helper.
- Separated outline bearings from bitmap trim offsets; cached offsets per glyph.
- Adjusted letter spacing to use font pixel-height units instead of line height.
- Cached HarfBuzz extents per glyph index to avoid per-draw recompute.
- Stored fixed-advance metrics in font units and scale at draw time.

## Details
### Unified TTF/HB scaling
- Added `font_ttf_metric_scale` for TTF/HB conversions; kerning, advances, and
  HarfBuzz layout now share the same conversion path.

### Bearings vs bitmap offsets
- `font_glyph_t` now stores `bearing_x/y` (outline metrics) plus
  `offset_x/y` (bitmap trim compensation), with `metrics_valid` to gate usage.
- Codepoint glyphs derive `offset_y = miny + bitmap_h - maxy` when the SDL_ttf
  surface height diverges from the outline metrics.
- HarfBuzz glyph indices cache bearings from HB extents and compute offsets
  from extents + bitmap height, keeping baseline alignment stable without
  overwriting bearings.

### Letter spacing base
- Letter spacing now uses the font pixel height as the base unit and scales
  through `font_ttf_metric_scale`, keeping spacing consistent when line-height
  overrides or font extents differ.

### Fixed advance precision
- Store `fixed_advance_units` from the glyph metrics and scale it at draw time
  with the same metric scale, reducing rounding drift in monospace layouts.

## Files
- `src/client/font.cpp`

## Notes / test ideas
- Verify UI font spacing and baseline with mixed-case text and descenders.
- Compare console monospace alignment across sizes and DPI scales.
- Validate HarfBuzz-shaped text (combining marks, ligatures) for stable
  baseline placement.
