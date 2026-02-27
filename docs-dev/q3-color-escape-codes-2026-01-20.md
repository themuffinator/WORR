# Quake 3 Color Escape Codes (2026-01-20)

## Summary
- Implemented Quake 3-style `^` color escapes across UI, console, HUD, and renderer string draws.
- Extended escape codes with `^a-^z` rainbow colors (uppercase supported).
- Updated width measurement to ignore escapes so alignment and wrapping stay correct.

## Escape Codes
- `^0-^7`: Standard Quake 3 colors (black, red, green, yellow, blue, cyan, magenta, white).
- `^a-^z`: Rainbow gradient (a = red, z = grey).
- Uppercase letters (`^A-^Z`) map to the same rainbow colors.

## Behavior Notes
- Escapes update the current draw color; the draw alpha is preserved from the caller.
- Invalid escapes (including `^` followed by a non-code character) render as normal text.
- Measurement helpers skip escapes, so widths are based on visible glyphs only.

## Implementation Details
- Added shared helpers in `src/common/utils.c` to parse and measure Q3 color escapes.
- `Font_DrawString` and `Font_MeasureString` now honor escapes for TTF, kfont, and legacy fonts.
- Renderer string paths (`R_DrawStringStretch`) interpret escapes in GL, VK, and VKPT backends.
- Console, screen, and cgame layout alignment paths use escape-aware width calculations.
