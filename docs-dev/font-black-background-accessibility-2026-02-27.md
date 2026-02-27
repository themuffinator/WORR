# Font Black Background Accessibility Option (2026-02-27)

## Summary
- Added a new client font cvar: `cl_font_draw_black_background`.
- When enabled (`1`), `Font_DrawString` now draws a solid black background rectangle behind each rendered text line before glyph draw.
- The option is disabled by default (`0`) and archived.

## Motivation
- Improve readability for UI/HUD/console text in bright or high-variance scenes.
- Provide a simple accessibility toggle that works at the shared font renderer level (legacy, kfont, and TTF code paths that route through `Font_DrawString`).

## Implementation Details
- File changed: `src/client/font.cpp`.
- New cvar state:
  - `static cvar_t *cl_font_draw_black_background`.
  - Registered via `Cvar_Get("cl_font_draw_black_background", "0", CVAR_ARCHIVE)`.
  - Value is clamped to `0..1` when checked.
- Added helper: `font_draw_string_black_background(...)`.
  - Splits input using the same `max_chars` byte budget and `UI_MULTILINE` newline behavior as the draw path.
  - Measures each line width via `Font_MeasureString(...)`.
  - Draws one black rectangle per line with scale-aware padding using `R_DrawFill32(..., COLOR_BLACK)`.
  - Uses the same vertical line step logic as text draw (`Font_LineHeight` plus pixel spacing).
- Hook point:
  - `Font_DrawString(...)` now conditionally calls `font_draw_string_black_background(...)` before drawing glyphs.

## Behavior Notes
- This is renderer-agnostic at the client font layer; both GL and Vulkan backends receive normal 2D fill draw calls.
- Backgrounds are line-based (not per-glyph), which keeps the option simple and predictable for multiline strings.
- Existing color escapes still work; only the background is forced to black.

## Validation
- Build verified with:
  - `meson compile -C builddir`
- Result:
  - `src/client/font.cpp` compiled successfully.
  - `worr.exe` linked successfully.
