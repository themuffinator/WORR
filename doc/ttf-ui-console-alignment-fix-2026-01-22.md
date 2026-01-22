# TTF UI/Console Alignment Fix (2026-01-22)

## Summary
- Corrected baseline placement for small font sizes (UI list and console) by
  using the font baseline scaled by glyph scale instead of a ratio derived
  from rounded line height.
- Prevented descender clipping by switching TTF glyph placement to
  floor/ceil rounding (positions and sizes).
- Stabilized console monospace spacing by deriving `fixed_advance` from
  glyph metrics even when SDL_ttf does not report a font as fixed width.

## Details
### Baseline placement
- **Problem:** The baseline used a `line_height * (baseline/extent)` ratio.
  When `line_height` is rounded (e.g., 5px UI list, 5px console), the baseline
  can shift by ~1px. That is enough to misalign short glyphs and descenders.
- **Fix:** Compute baseline offset as:
  `baseline_offset = font_baseline * glyph_scale`
  so the baseline is derived from the same scale used for glyph metrics.

### Glyph rounding
- **Problem:** Rounding to nearest (`Q_rint`) can clip a final row/column for
  tiny glyphs, which looks like missing descenders or uneven top alignment.
- **Fix:** Use `floor` for glyph positions and `ceil` for glyph sizes to keep
  the full bitmap on screen.

### Console fixed advance
- **Problem:** `fixed_advance` stayed at the requested size when
  `TTF_FontIsFixedWidth()` returned false, yielding incorrect spacing for
  monospace fonts that are not flagged correctly.
- **Fix:** If `fixed_advance` is requested, always derive it from the `M`
  glyph’s advance (scaled to the virtual line height) when available.

## Files
- `src/client/font.cpp`
