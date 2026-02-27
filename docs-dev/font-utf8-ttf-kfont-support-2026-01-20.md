# UTF-8/TTF Font Support (2026-01-20)

## Summary
- Added a unified font loader that supports TTF/OTF, kfont, and legacy PCX with UTF-8 rendering via SDL3_ttf.
- Wired new font paths into console, menu, and centerprint usage with per-cvar control and fallback chaining.
- Preserved existing HUD sizing, alignment, and drop shadow behavior while improving DPI clarity.
- Added Quake 3-style color escape parsing to the font path, including a-z rainbow codes.

## Cvars and Defaults
- `con_font`: `fonts/RobotoMono-Regular.ttf`
- `ui_font`: `fonts/NotoSansKR-Regular.otf`
- `cl_font`: `fonts/RussoOne-Regular.ttf`

Each cvar accepts a TTF/OTF, `.kfont`, or legacy font image path.

## Fallback Chain
If a TTF/OTF fails to load, the loader falls back in order:
1) `fonts/qfont.kfont`
2) `conchars.png`

Missing glyphs in a valid TTF/OTF also fall back to kfont and then legacy.

## Engine Integration
- Added a font subsystem (`Font_*`) that handles UTF-8 decoding, glyph atlas management, and renderer uploads.
- Console and screen text now route through `Font_DrawString`/`Font_MeasureString` when using the new fonts.
- Menu UI uses `UI_Font*` helpers to draw, measure, and provide a legacy handle for cursor glyphs.

## Scaling and DPI
- Font pixel sizes scale from the virtual 640x480 basis and respect `cl_scale`, `con_scale`, and `ui_scale`.
- The loader snaps to integer pixel scale to keep text crisp at high DPI.

## UI Notes
- JSON menu globals now support `$`-prefixed cvar references (e.g., `"font": "$ui_font"`).
- Input fields render with the new fonts while cursor glyphs continue to use the legacy handle for consistency.

## Color Escapes
- Strings drawn through `Font_DrawString`/`Font_MeasureString` now understand `^0-^7` Quake 3 colors plus `^a-^z` rainbow codes (uppercase supported).
- Escape codes are skipped by string measurement, so alignment and wrapping stay consistent.
- The base alpha passed to the draw call is preserved across color changes.
