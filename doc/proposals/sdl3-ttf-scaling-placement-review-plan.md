# SDL3_ttf scaling & placement review plan (WORR)

## Context
The current SDL3_ttf pipeline in `src/client/font.cpp` mixes SDL_ttf glyph metrics, SDL-rendered bitmap sizes, and HarfBuzz shaping data to determine glyph positions. The task is to scrutinize the accuracy of scaling and placement, then compare against Daemon Engine’s high-quality implementation to identify gaps and fixes.

## Access blocker
I attempted to fetch Daemon Engine sources from GitHub for a direct comparison, but network access to GitHub is blocked in this environment (HTTP 403 via proxy). To complete the comparison step, I need either:
- A local copy of the Daemon Engine repository, or
- The specific files/paths (or excerpts) from Daemon that implement font rendering, layout, and glyph placement.

Until those references are available, the items below are based strictly on a review of WORR’s current implementation and are marked as **pre-comparison**.

## Pre-comparison findings (WORR SDL3_ttf implementation)
These are concrete issues and improvement candidates found by reading the current WORR code. Each item should be cross-checked against Daemon once the reference is available.

### 1) Mixed units between HarfBuzz advances and WORR scaling
- HarfBuzz is configured with `hb_font_set_scale(hb_font, pixel_height * 64, pixel_height * 64)` and positions are read as 26.6 values (`x_advance / 64`).
- The layout then multiplies those advances by `glyph_scale`, which is derived from WORR’s `unit_scale` and a global `k_font_scale_boost`.
- Because `unit_scale` is based on `virtual_line_height / ttf.extent`, but HarfBuzz advances are already in pixel-height units, this creates a risk of double-scaling or inconsistent scaling when the font’s line height and `extent` do not match the pixel height or line skip.

**Potential improvement:** Introduce a single authoritative conversion from HarfBuzz units to screen pixels (or replace `glyph_scale` with an explicit HarfBuzz unit scale) so that glyph advances and extents are scaled with the same metric basis as SDL_ttf metrics.

### 2) Baseline offset uses ascent but is adjusted per glyph using bitmap height
- For non-HB drawing, baseline is `baseline_y = y + ttf.baseline * glyph_scale`, with per-glyph `bearing_y` derived from SDL_ttf metrics.
- For HarfBuzz shaping, `bearing_y` is derived from HarfBuzz extents but then replaced with `hb_miny + cached->h` when a cached bitmap exists, effectively mixing outline metrics with rasterized bitmap height.
- This can shift glyphs vertically depending on the rendered bitmap size rather than the font’s outline metrics, which risks inconsistent baseline alignment across glyphs and sizes.

**Potential improvement:** Use a consistent baseline derived from font metrics (ascent/descent) and apply glyph-specific bearings from the same metric system (either all HarfBuzz extents or all SDL_ttf metrics), avoiding conditional bitmap-height overrides.

### 3) Rasterized bitmap size vs. outline metrics divergence
- `font_get_ttf_glyph` uses `TTF_GetGlyphMetrics` for bearings, then renders a blended glyph surface and overwrites `bearing_y` to `miny + surface->h` when the rasterized height differs.
- This implies rasterization trimming affects baseline alignment.

**Potential improvement:** Keep bearings from outline metrics and track bitmap offsets separately (e.g., store render offsets distinct from bearings) to avoid trimming-induced baseline shifts.

### 4) Rounding strategy could introduce jitter
- Glyph draw positions use mixed `floorf/ceilf` and integer rounding for glyph bounds while advances are applied in float space. This can cause uneven placement, especially with fractional scaling and letter spacing.

**Potential improvement:** Standardize rounding (e.g., consistent floor/round on pen position or use subpixel positioning with shader support) to reduce jitter.

### 5) Letter spacing uses line-height scaling
- Letter spacing is computed as `line_height * letter_spacing`, which may vary with line-height overrides and not directly with the font’s em size or HarfBuzz metrics.

**Potential improvement:** Consider using font em size, HarfBuzz scale, or a fixed pixel-based spacing to ensure consistent visual spacing across fonts and sizes.

### 6) Fixed advance calculations derived from glyph metrics
- Fixed advance uses metrics from glyph ‘M’ scaled by `virtual_line_height / ttf.extent`, and then `k_font_scale_boost` is applied during draw.

**Potential improvement:** Verify fixed advance calculations against the actual font units used by HarfBuzz/SDL_ttf, and confirm it matches Daemon’s approach for monospace spacing.

## Comparison plan with Daemon Engine
Once Daemon’s font implementation is available, execute the following comparisons:

1) **Locate font rendering code**
   - Identify where Daemon performs glyph loading, metric extraction, shaping, and rasterization.
   - Capture how it defines line height, ascent/descent, and baseline.

2) **Compare unit conversions**
   - Map Daemon’s unit conversions (FT/HB units → pixels) to WORR’s `unit_scale`, `glyph_scale`, and `pixel_height` usage.
   - Look for double-scaling or mismatched assumptions.

3) **Compare glyph placement logic**
   - Identify how Daemon computes `x`/`y` for glyphs, especially baseline placement and bearing offsets.
   - Compare with WORR’s per-glyph baseline/bearing adjustments and confirm if Daemon avoids bitmap-driven bearing overrides.

4) **Compare rounding and spacing behavior**
   - Review Daemon’s rounding strategy (subpixel vs integer alignment) and how it avoids jitter.
   - Compare letter spacing treatment and kerning usage.

5) **Gap analysis and fixes**
   - Enumerate concrete deviations that would cause visible placement errors or incorrect scaling in WORR.
   - Propose fix list in priority order (baseline correctness, unit scaling, rounding consistency, spacing consistency).

## Deliverables
- A delta list of WORR vs Daemon differences in scaling and placement.
- A prioritized fix plan with code-level change candidates.
- A validation checklist for glyph alignment, baseline stability, and spacing across sizes.

## Next input needed
Please provide access to Daemon Engine’s font rendering implementation (specific file paths or excerpts) so the comparison can be completed.
