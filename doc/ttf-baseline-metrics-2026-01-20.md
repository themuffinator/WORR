# TTF Baseline Alignment Using SDL_ttf Metrics

**Date:** 2026-01-20  
**Scope:** `src/client/font.cpp`

## Problem
Per-glyph baselines still drifted across a line. This happens because
SDL_ttf renders each glyph into a surface whose top is offset by the
glyph's **min Y** (the render pipeline compensates for negative top
values), so the surface origin is not always at a constant distance
from the baseline.

## Research Notes
- SDL_ttf `TTF_GetGlyphMetrics` returns `maxy` (`sz_top`) relative to
  the baseline; this is the distance from the baseline to the glyph top.
- SDL_ttf uses `TTF_Size_Internal` to compute `miny/maxy` and `ystart`.
  When `maxy` exceeds the font ascent, `ystart` shifts the surface so
  the glyph fits, which means the baseline offset becomes `max(ascent, maxy)`.
- The FreeType tutorial confirms the baseline-relative nature of glyph
  metrics (bearing and bbox) and how those map to pixel placement.

## Fix
Compute a **baseline offset** that mirrors SDL_ttf's render offset:

```
baseline_offset = max(font_ascent, glyph_maxy)
draw_y = baseline_y - baseline_offset * glyph_scale
```

This keeps the baseline constant for normal glyphs (draws at line top)
and shifts only glyphs that exceed ascent, matching SDL_ttf's internal
line placement.

## Expected Result
Glyphs on the same line share a consistent baseline, regardless of
glyph-specific top extents.
