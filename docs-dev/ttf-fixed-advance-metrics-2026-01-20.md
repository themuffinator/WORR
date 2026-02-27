# TTF Fixed Advance From Glyph Metrics

**Date:** 2026-01-20  
**Scope:** `src/client/font.cpp`

## Problem
Console glyphs rendered with TTF/OTF appeared overly spaced. The console
was passing a fixed advance derived from the line height, which does not
match the font’s actual advance width. This made horizontal spacing larger
than the font’s intended metrics.

## Change
When loading a fixed‑width TTF/OTF font:
- Query the glyph advance for `'M'` via `TTF_GetGlyphMetrics`.
- Convert the advance from font pixels to virtual units using
  `virtual_line_height / extent`.
- Replace `font->fixed_advance` with this derived value.

This keeps monospace spacing consistent with the font’s real advance
width while preserving the configured virtual line height.

## Result
TTF/OTF horizontal spacing matches the font’s intended metrics and no
longer appears artificially expanded by console sizing.
