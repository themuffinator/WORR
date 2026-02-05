# Font Glyph Dump Command (2026-02-05)

## Summary
- Adds a client console command to dump per-glyph metrics for the standard
  ASCII range (32–126).
- Loads fonts at a requested virtual line height (default 32) and writes a
  log with coordinates, bounds, and metric deltas.

## Command
- `font_dump_glyphs`
- `font_dump_glyphs <size>`
- `font_dump_glyphs <size> <outfile>`
- `font_dump_glyphs <font_path> [size] [outfile]`

When no font path is specified, the command dumps these standard fonts:
- `con_font`
- `cl_font`
- `ui_font`
- `fonts/AtkinsonHyperLegible-Regular.otf` (scr UI default)

Output files are written to `fontdump/` with a `.txt` extension. If no
`outfile` is provided, an auto-numbered file is created.

## Output Format
Each font section includes:
- Font configuration (kind, line height, pixel scale, baseline/extent).
- A row per codepoint with:
  - Advance and scaled advance.
  - SDL_ttf metrics (`minx/maxx/miny/maxy`).
  - Cached glyph bearings and offsets.
  - Bitmap bounds in baseline space.
  - Deltas between bitmap bounds and SDL_ttf metrics.

This helps detect baseline shifts, padding issues, and spacing drift between
SDL_ttf metrics and cached glyph geometry.

## Files Updated
- `src/client/font.cpp`
