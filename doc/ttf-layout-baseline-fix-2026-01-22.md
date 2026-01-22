# TTF layout revision for baseline + spacing (2026-01-22)

## Summary
Reworked the Harfbuzz-rendering path to use SDL3_ttf text layout output (TTF_Text draw ops) instead of manual hb glyph extents. This keeps SDL3_ttf?s baseline/kerning decisions aligned with the glyph bitmaps and prevents per-glyph top-alignment drift.

## Key changes
- TTF layout path:
  - Build a `TTF_Text` layout (engine null) to get per-glyph `dst/src` rects and glyph indices.
  - Use layout `dst` positions (scaled to the engine?s virtual line height) for draw placement.
  - Use layout `src` rects when sampling cached glyphs from the atlas.
- Spacing controls:
  - Letter spacing now offsets layout glyph positions in the draw pass; measure uses the same logic for width.
  - Fixed-advance fonts (console) bypass the Harfbuzz layout path to keep strict monospace spacing and avoid ligature-driven width drift.
- Multiline handling:
  - Trimmed shaping output to a single line when `UI_MULTILINE` is not set.
  - Line breaks are tracked via byte offsets so color codes still map correctly after shaping.

## Files updated
- `src/client/font.cpp`
  - Added SDL3_ttf layout helpers for text measurement and draw ops.
  - Updated Harfbuzz draw/measure path to consume SDL3_ttf layout ops and atlas glyph regions.
  - Skipped Harfbuzz layout when `fixed_advance > 0` to preserve console monospace behavior.

## Build status
- `meson compile -C builddir` completes without compiler warnings.
- The only remaining output is the known ninja warning about a truncated `.ninja_deps/.ninja_log`.
