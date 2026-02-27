# Harfbuzz TTF Baseline/Spacing Revision (2026-01-22)

## Summary
- Replaced the SDL_ttf text layout ops path with direct Harfbuzz shaping for TTF rendering.
- Re-aligned baseline math to match the non-Harfbuzz TTF path using ascent/extent ratios.
- Switched horizontal spacing to Harfbuzz advances plus explicit letter spacing.

## Implementation Details
- Added a Harfbuzz line shaping helper that builds buffers per line and uses
  `hb_buffer_guess_segment_properties` with monotone cluster mapping.
- Each line now computes:
  - `baseline_y = y + line_height * (ascent / extent) + line_index * line_step`
  - `pen_x` advanced by Harfbuzz `x_advance` (scaled by `font_draw_scale`).
- Glyph placement uses Harfbuzz extents for bearings and Harfbuzz offsets:
  - `draw_x = pen_x + (x_offset + x_bearing) * glyph_scale`
  - `draw_y = baseline_y - (y_offset + y_bearing) * glyph_scale`
- Glyphs are still sourced from the SDL_ttf glyph image cache by glyph index,
  with a pre-pass to populate the atlas and a flush before drawing.
- Fallbacks use the top-of-line `line_y` for kfont/legacy to preserve existing
  legacy alignment behavior.

## Measurement
- Width now sums Harfbuzz `x_advance` values (plus letter spacing) per line.
- Height remains `lines * line_height + (lines - 1) * pixel_spacing`.

## Notes / Follow-ups
- Harfbuzz extents are used for bearing placement; if hinted bitmap placement
  still shows drift at very small sizes, consider sampling FreeType glyph
  metrics directly to match rasterized bearings.
- RTL scripts are shaped correctly but still assume a left-origin pen position;
  if RTL alignment becomes a requirement, compute an initial pen offset based
  on total line advance and draw right-to-left.

## Testing Guidance
- Verify baseline alignment on mixed-case strings (AaBbCc...), digits, and
  punctuation in `cl_font` and UI text.
- Check color-coded strings and multi-line text for consistent spacing.
- Compare against kfont/legacy fallbacks to confirm consistent vertical
  alignment when glyphs are missing.
