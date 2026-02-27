# Font Baseline + Console Style Update (2026-01-20)

## Summary
Adjusted font scaling/placement to fix baseline alignment and sized the
console layout to match the new font scale. The console background now
renders a Quake II Rerelease-style look when using the default background.

## Font Changes
- Added a global 1.5x draw boost to match the target visual scale.
- Added `font_draw_scale()` to apply unit scaling, DPI compensation, and the
  1.5x boost consistently across TTF/kfont/legacy draw, measure, and kerning.
- TTF draw placement now computes `baseline_y = y + ascent` and blits with
  `baseline_y - glyph_h` to align glyphs to the baseline cell.
- Restored unit scale math to include `pixel_scale`, then divided in draw
  to render crisp text in virtual units.

## Console Layout + Style
- Console row height/width now comes from the active font size, so line
  spacing and cursor placement track the scaled font.
- Prompt and cursor draw with stretch scaling so they match the TTF size.
- Default console background renders a dark rerelease-style gradient with
  a separator and accent line, and version/clock text uses warm amber.

## Files Touched
- `src/client/font.cpp`
- `src/client/console.cpp`
