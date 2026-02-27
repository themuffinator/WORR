# TTF Virtual-Scale Debug Toggle (2026-02-05)

## Summary
- Added `cl_font_skip_virtual_scale` to bypass the virtual-screen derived pixel scale
  when loading fonts (console, HUD, and UI fonts).
- Set console `con_fontsize` default to `8` to restore the intended baseline size
  while keeping `con_fontscale` available for overrides.

## Motivation
Recent baseline improvements made the TTF output more consistent, but debug overlays
still show glyphs sitting below the baseline guides in some cases. A common source of
mismatch is the interaction between:
- **Virtual screen scaling** (integer base scale derived from 640x480), and
- **TTF rasterization** at a pixel size derived from that scale.

The new toggle lets us isolate whether the virtual scale factor is the root of the
baseline mismatch by rendering fonts at a 1:1 pixel scale relative to the requested
virtual line height.

## New Cvar
- `cl_font_skip_virtual_scale` (default `0`, archived)
  - `0`: Use the current virtual-screen-derived pixel scale.
  - `1`: Ignore the virtual screen scale; use only user UI scale (`scr_scale`) or
    console scale (`con_scale`) when computing font pixel scale.

This is applied to:
- Console fonts (`Con_GetFontPixelScale`)
- HUD fonts (`SCR_GetFontPixelScale`)
- UI fonts (`UI_FontCalcPixelScale`)

## Expected Observations
- If baseline mismatch improves with `cl_font_skip_virtual_scale 1`, the issue is
  likely tied to integer virtual scaling and rounding in the raster size.
- If it does not improve, the mismatch is likely in bearing/offset math or in
  rendered-metric selection (outline vs rendered).

## Test Steps
1. `cl_font_debug_draw 27`
2. Capture baseline alignment with default settings.
3. Set `cl_font_skip_virtual_scale 1` and reload UI/console.
4. Compare baseline alignment and glyph bounds.

## Files Updated
- `src/client/console.cpp`
- `src/client/screen.cpp`
- `src/client/ui_font.cpp`
