# Console Baseline + Opacity + Layout Tweaks

**Date:** 2026-01-20  
**Scope:** `src/client/font.cpp`, `src/client/console.cpp`

## Summary
- Re-aligned TTF baseline to the active line height to prevent drift.
- Set the default console open height to 0.66.
- Locked console background opacity to 0.8 for consistent visuals.
- Moved version info to the line above the input prompt.

## Changes
### Baseline alignment
`font_draw_ttf_glyph_cached` now derives the baseline from the current
line height (`Font_LineHeight`) using the font baseline ratio
`baseline/extent`. This keeps glyph placement aligned to the same
rounded line height used for line spacing.

### Console height default
`con_height` default is now `0.66`.

### Background opacity
All console background fills use a fixed alpha of 204 (0.8 * 255),
including the rerelease-style gradient and custom background paths.

### Version placement
Product version text now draws at `vislines - prestep` so it appears
above the input line.

## Verification
1. Open the console and confirm the background opacity stays constant.
2. Confirm version info appears above the input prompt.
3. Check multiple lines of text for consistent baseline alignment.
