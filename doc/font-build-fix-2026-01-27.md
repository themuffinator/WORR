# Font build fix 2026-01-27

## Summary
- Removed a duplicate `snap_positions` variable declaration in HarfBuzz draw path.
- Added a forward declaration for `font_codepoint_from_slice` before first use.
- Restored nominal-metrics locals in `font_get_ttf_glyph_index` and removed a stray `nominal_codepoint` reference in `font_get_ttf_glyph`.

## Details
- The HarfBuzz text draw routine defined `snap_positions` twice, tripping a redefinition error; now it is defined once.
- `font_codepoint_from_slice` is defined later in the file; a forward declaration keeps the HarfBuzz shaping path valid.
- `font_get_ttf_glyph_index` now has the nominal metrics locals needed by the HarfBuzz nominal metrics branch.
- The `font_get_ttf_glyph` path had a `nominal_codepoint` stub that referenced a non-existent parameter; it is removed to keep non-HarfBuzz builds safe.
- Added missing snap_positions in HB width measurement path to match draw path.
- Declared snap_positions in HarfBuzz width measurement to keep snapping consistent.
