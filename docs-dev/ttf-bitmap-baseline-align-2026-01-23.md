# TTF/OTF Bitmap Baseline Alignment (2026-01-23)

## Summary
Adjusted TTF/OTF glyph placement to keep the baseline aligned when glyph
bitmaps are shorter than their typographic bounds. This removes per-glyph
vertical drift for mixed-height characters.

## Problem
SDL3_ttf returns glyph metrics (miny/maxy) from the outline, but the rendered
bitmap can be shorter due to hinting or trimmed rows. Using `maxy` directly
places the bitmap too high or low depending on the glyph, so mixed-height
characters appear to sit on different baselines.

## Fix
- For the non-HarfBuzz path, derive the per-glyph top bearing from the bitmap
  height: `bearing_y = miny + surface->h`.
- For the HarfBuzz path, adjust the bearing from glyph extents using the cached
  bitmap height so draw placement uses the same metric space as the atlas.

This keeps the line baseline derived from the font ascent while aligning each
bitmap's top edge to its true rendered bounds.

## Files Updated
- `src/client/font.cpp`
