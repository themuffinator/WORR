# TTF Glyph Spacing Overhaul (2026-02-27)

## Goal
Completely revise TTF glyph spacing and measurement so glyph placement is stable, professional, and consistent under WORR virtual-screen scaling.

## External References Used
- GPL SDL3_ttf integration reference: [FrodeSolheim/fs-uae](https://github.com/FrodeSolheim/fs-uae) (GPL-2.0), specifically `od-fs/fsgui/fsgui-font.c` usage patterns for SDL3_ttf text sizing/drawing integration.
- Glyph metric/layout model port target: [libsdl-org/SDL_ttf](https://github.com/libsdl-org/SDL_ttf), primarily `src/SDL_ttf.c` (`TTF_GetGlyphMetrics`, `TTF_GetGlyphKerning`, `CollectGlyphs*`, `TTF_Size_Internal`).

## What Changed In WORR

### 1. Glyph advance model moved to SDL_ttf-style metrics
File: `src/client/font.cpp`

- Added explicit per-glyph `advance_26_6` storage in `font_ttf_glyph_t`.
- Added `fixed_advance_26_6` storage in `font_ttf_t`.
- Reworked advance computation to use 26.6 fixed-point semantics, then scale once at draw/measure time.
- Removed the previous `+1` advance inflation path that was introducing cumulative over-spacing.

Impact:
- No more systematic extra gap per glyph.
- Spacing now follows actual font metrics instead of padded approximations.

### 2. Glyph box extraction aligned with professional FreeType/SDL_ttf behavior
File: `src/client/font.cpp`

- Replaced padded box computation (`-64/+64` guard band) with SDL_ttf-style metric bounds:
  - `left = floor(horiBearingX)`
  - `right = ceil(horiBearingX + width)`
  - `top = floor(horiBearingY)`
  - `bottom = top - ceil(height)`

Impact:
- Removed artificial transparent side growth that widened visual spacing.
- Tightened ink bounds while keeping baseline alignment stable.

### 3. Kerning mode updated to unfitted 26.6 flow
File: `src/client/font.cpp`

- Switched kerning query mode to `FT_KERNING_UNFITTED` for layout-space kerning consistency.
- Kerning now follows the same “26.6 -> scaled once” flow as advance.

Impact:
- Better consistency between measurement and rendered placement.
- Reduced hinting/rounding drift in pair spacing.

### 4. Draw path revised for zero-advance glyph correctness
File: `src/client/font.cpp`

- Removed forced fallback pen movement for zero-advance glyphs.
- Preserved previous kerning context across zero-advance marks instead of hard-resetting pair state.
- Fixed fixed-width centering to use true glyph advance metrics (not inflated skip).

Impact:
- Combining marks and zero-advance glyphs no longer inject fake spacing.
- Monospaced/fixed cell rendering centers glyphs more cleanly.

### 5. Measurement path rewritten to use glyph ink bounds + pen extents
File: `src/client/font.cpp`

- Replaced pure “sum of advances” width logic with SDL_ttf-like line bounds behavior:
  - track per-line `pen_x`
  - track glyph ink bounds (`line_min_x`, `line_max_x`)
  - include pen extents for trailing whitespace behavior
  - commit line width from bounds at newline/end
- Kept draw/measure kerning+letter-spacing sequencing aligned.

Impact:
- Width reports now match rendered text much more closely.
- Negative bearings and tight glyph extents are measured correctly.

## Virtual Screen Scaling Safety

The revised spacing pipeline still relies on WORR’s existing scale path:
- load in font metric space,
- apply `font_draw_scale()` exactly at draw/measure,
- keep `pixel_scale` / virtual scaling behavior intact.

No OpenGL/Vulkan renderer fallback changes were introduced. This is client-side font math only.

## Build + Staging Verification

Executed:
- `meson compile -C builddir`
- `meson install -C builddir`

Results:
- Build completed successfully.
- `.install/` refreshed with current binaries/assets per repo policy.
