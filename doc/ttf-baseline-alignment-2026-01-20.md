# TTF Baseline Alignment Fix (2026-01-20)

## Issue
- TTF text was consistently offset vertically relative to legacy console
  characters and the prompt/cursor drawn with `conchars.png`.

## Cause
- TTF scaling used the font's `line_skip`, which includes the font's line-gap.
  This introduced extra top padding in the 8px console line height.
- Baseline positioning used the font ascent directly, so glyphs that do not
  reach the full ascent (most glyphs) appeared lower than expected.

## Fix
- Compute glyph extents (`min_y`/`max_y`) across the ASCII set (32-126) at
  load time.
- Use the extent height for scaling so glyphs fill the virtual line height
  without line-gap padding.
- Use the computed `baseline` (max_y) for vertical placement so ASCII glyphs
  align with the legacy console cell.

## Result
- TTF glyphs align with legacy console prompt/cursor.
- Consistent top/bottom fit within the 8px console line height.
