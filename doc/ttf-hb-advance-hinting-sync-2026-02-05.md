# TTF HarfBuzz Advance Sync (2026-02-05)

## Summary
- Proportional (HarfBuzz-shaped) text could drift horizontally because
  HarfBuzz advances and SDL_ttf hinted advances differ by small but visible
  amounts.
- Adjust HarfBuzz advances to match SDL_ttf's hinted nominal advance while
  preserving kerning and shaping offsets.
- Apply the same adjustment in the HB measurement path so centering and
  alignment match draw results.

## Root Cause
HarfBuzz positions are derived from the font outline metrics, while SDL_ttf
renders hinted glyphs and reports advances based on those hinted outlines.
For proportional fonts, the HB x_advance values can differ from SDL_ttf's
advance units, producing uneven spacing or overall width shifts relative to
UI layout.

## Fix
- For each HarfBuzz glyph, compute the nominal HB advance
  (`hb_font_get_glyph_h_advance`) and compare it to SDL_ttf's nominal advance.
- Apply the delta to the shaped advance:
  `adjusted = hb_advance + (ttf_advance - hb_nominal)`.
  This keeps kerning from HarfBuzz intact while syncing to SDL_ttf's hinted
  metrics.
- Use cached SDL_ttf metrics when available; fall back to
  `TTF_GetGlyphMetrics` when measuring.

## Validation
- Use `cl_font_debug_draw 27` on UI/scr strings and compare spacing before/after.
- Centered UI labels should now line up with their measured widths.

## Files Updated
- `src/client/font.cpp`
