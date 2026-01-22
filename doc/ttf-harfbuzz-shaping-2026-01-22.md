# TTF HarfBuzz Shaping + Baseline Alignment (2026-01-22)

## Summary
- Added HarfBuzz-based shaping for TTF/OTF text, including ligatures and
  kerning, while preserving Quake color codes and the existing atlas renderer.
- Switched the TTF baseline computation to use the line-height ratio
  (`baseline/extent`) so glyph placement aligns consistently with the active
  line height.

## HarfBuzz Integration
- New HarfBuzz font objects are created from the loaded TTF/OTF bytes and
  sized to the same pixel height as SDL3_ttf.
- Text is shaped per line; glyph indices and positions come from HarfBuzz.
- Glyph bitmaps are cached in the atlas using `TTF_GetGlyphImageForIndex`,
  keyed by glyph index instead of Unicode codepoint.
- Color escape codes are stripped before shaping and then re-applied by
  mapping HarfBuzz cluster offsets back to per-byte color state.

## Fixed-Advance Behavior
- When `fixed_advance` is set (console monospace), kerning/ligature features
  are disabled to preserve fixed-width alignment.
- For proportional fonts (UI/screen), HarfBuzz features remain fully enabled.

## Baseline Alignment
- Baseline placement now derives from the line-height ratio:
  `baseline_y = y + line_height * (baseline/extent)`.
- This keeps glyph baselines aligned with the same rounded line height used
  for line spacing, removing the previous drift/top-alignment artifacts.

## Build Changes
- Added HarfBuzz as a Meson dependency with a minimal feature set and linked
  it into the client.
- SDL3_ttf is now able to pick up HarfBuzz via the override dependency.
- New config define: `USE_HARFBUZZ`.

## Files Updated
- `meson.build`
- `src/client/font.cpp`
