# TTF Glyph Trimming + Atlas Oversize Guard (2026-01-22)

## Summary
- Trim transparent rows from SDL3_ttf glyph surfaces before packing into the
  atlas.
- Adjust per-glyph `bearing_y` to match the trimmed top row so baselines align
  with visible pixels.
- Skip atlas placement for glyphs that exceed the page size and fall back to
  kfont/legacy rendering.

## Details
- `font_find_glyph_rows` scans RGBA32 glyph surfaces for the first/last rows
  containing non-zero alpha.
- `font_get_ttf_glyph` uses the scan to set `src_row_offset`, shrink `glyph.h`,
  and reduce `bearing_y` by the trimmed top offset.
- Oversize guard compares trimmed glyph dimensions to
  `k_atlas_size - 2 * k_atlas_padding`; oversized glyphs are flagged and not
  copied into the atlas.
- The draw path skips oversized TTF glyphs so fallback fonts render them,
  keeping glyph placement stable without atlas overflows.

## Files Updated
- `src/client/font.cpp`
