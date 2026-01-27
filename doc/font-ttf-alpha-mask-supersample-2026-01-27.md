# SDL3_ttf Alpha Mask & Supersampling Updates (2026-01-27)

## Summary
Adjusted SDL3_ttf glyph handling to render as proper alpha masks, added optional
TTF hinting/supersample controls, and tightened rounding in the Harfbuzz draw
path to avoid losing edge pixels (descenders at small sizes).

## Changes

### Alpha-mask atlas uploads
- TTF atlas pages now initialize and clear with **RGB=255, A=0** so the atlas
  behaves like a coverage mask instead of color data.
- Glyph uploads copy **alpha-only** by default (RGB forced to 255, alpha from the
  glyph). SDL3_ttf color glyphs are preserved when the image type reports
  `TTF_IMAGE_COLOR`.
- TTF atlas images are registered with `IF_NO_COLOR_ADJUST` to avoid gamma
  scaling of glyph colors.

### Optional TTF hinting + supersampling
- Added `cl_font_ttf_hinting` (default `1` = light). Values:
  - `0` = none
  - `1` = light
  - `2` = mono
  - `3` = normal
- Added `cl_font_ttf_supersample` (default `1`, range `1..4`), which multiplies
  the SDL3_ttf pixel height at load time. The rendering scale compensates so the
  line height stays consistent while glyph coverage becomes denser.

### Rounding safety in Harfbuzz path
- Harfbuzz glyph region draws now **floor positions** and **ceil sizes** to
  avoid clipping the last pixel row/column at small scales.

## Files Touched
- `src/client/font.cpp` (TTF atlas init/clear, alpha-mask upload, hinting and
  supersample cvars, Harfbuzz rounding)

## Suggested Tests
- Render console/UI text at small sizes and verify descenders (`g`, `p`, `q`,
  `y`) no longer disappear.
- Toggle `cl_font_ttf_hinting 0/1/2/3` and confirm spacing remains stable.
- Try `cl_font_ttf_supersample 2` and confirm text sharpness improves without
  layout drift.
