# UI/SCR TTF Letter Spacing (2026-01-20)

## Summary
- Add a minimal, consistent amount of tracking for scr/ui TTF/OTF fonts.
- Keep kfont/legacy spacing unchanged.

## Behavior
- A per-font `letter_spacing` factor is introduced and applied only when a font
  is rendered as TTF.
- The spacing is computed as a fraction of the current line height and inserted
  between consecutive TTF glyphs (after kerning).
- UI and screen fonts set the spacing factor to 0.06 to yield a subtle, usually
  1-pixel gap at typical line heights.

## Implementation Details
- New API: `Font_SetLetterSpacing(font_t *font, float spacing)`.
- `Font_DrawString` and `Font_MeasureString` add tracking between adjacent TTF
  glyphs when `letter_spacing > 0`.
- `cl_font` and `ui_font` apply the spacing factor after font load.

## Files Updated
- `src/client/font.cpp` (letter spacing storage and render/measure logic)
- `inc/client/font.h` (new API declaration)
- `src/client/screen.cpp` (apply spacing to `cl_font`)
- `src/client/ui_font.cpp` (apply spacing to `ui_font`)
