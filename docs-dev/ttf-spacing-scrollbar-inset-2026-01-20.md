# TTF Spacing Precision + Console Scrollbar Inset (2026-01-20)

## Summary
- Fix TTF glyph spacing by switching to float accumulation for advances/kerning.
- Avoid injecting letter spacing for zero-advance TTF glyphs (combining marks).
- Keep right-aligned console text clear of the scrollbar.

## TTF Spacing Changes
- Kerning is now applied as a float (no per-glyph rounding).
- Advances are scaled as floats and accumulated in `Font_DrawString` and
  `Font_MeasureString`, rounding only when converting to screen pixels.
- Zero-advance TTF glyphs no longer force a minimum 1px advance, preventing
  extra gaps for combining marks.
- Letter spacing applies only when the current TTF glyph has a non-zero advance.

## Console Scrollbar Inset
- When the scrollbar is visible, a `right_edge` inset is computed from the bar
  position, and clock/version alignment use this inset so they never overlap the
  bar.

## Files Updated
- `src/client/font.cpp`
- `src/client/console.cpp`
