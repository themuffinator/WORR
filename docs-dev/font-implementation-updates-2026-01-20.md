# Font Implementation Recommendations Applied (2026-01-20)

## Summary
Implemented the recommendations from `docs-dev/proposals/font-implementation-analysis-2026-01-20.md` with
batched TTF atlas uploads, glyph cache LRU eviction, kerning support, ASCII pre-rasterization, and
shared color escape parsing across client and renderer.

## Changes

### TTF atlas upload batching
- Added per-page dirty tracking in `src/client/font.cpp` so glyph rasterization marks the atlas
  page dirty instead of immediately uploading a full 1024x1024 texture per glyph.
- Introduced a prepass (`font_prepare_ttf_glyphs`) before `Font_DrawString` that loads needed glyphs
  for the string and then flushes any dirty pages once.
- Added a safety flush for any dirty page at draw time so newly-created glyphs still render
  immediately even under cache eviction pressure.

### Glyph cache LRU eviction
- Implemented an LRU list for TTF glyphs (`font_ttf_t::lru`) and a new cvar `cl_font_glyph_cache_size`
  (default 2000) to cap the glyph cache size.
- On overflow, the least-recently-used glyph is evicted; if a suitable evicted slot fits a new
  glyph, it is reused to avoid unnecessary atlas growth when possible.

### Kerning support
- Added `TTF_GetGlyphKerning` integration and applied kerning in both `Font_DrawString` and
  `Font_MeasureString` for consecutive TTF glyphs.
- Kerning is skipped when `fixed_advance` is in use to preserve fixed-width console alignment.

### Const-correct metrics (no side effects during measurement)
- `font_advance_for_codepoint` now uses `TTF_GetGlyphMetrics` via `font_get_ttf_advance_units`,
  avoiding `const_cast` and rasterization during string measurement.

### ASCII pre-rasterization
- On TTF load, ASCII glyphs (32-126) are pre-rasterized and flushed to the atlas, removing the
  first-frame hitch for common UI/console text.

### Unified color escape parsing
- Moved `Com_ParseColorEscape`, `Com_HasColorEscape`, and `Com_StrlenNoColor` to inline shared
  implementations in `inc/common/utils.h`.
- Updated `src/rend_gl/draw.c` and `src/rend_vk/vkpt/draw.c` to use the shared utilities instead of
  local duplicate parsing logic.

## Files Touched
- `src/client/font.cpp` (atlas dirty tracking, LRU cache, kerning, prepass, ASCII preload)
- `inc/common/utils.h` and `src/common/utils.c` (shared inline color parsing)
- `src/rend_gl/draw.c` and `src/rend_vk/vkpt/draw.c` (use shared color parsing)

## Notes
- Atlas uploads are still full-page (no sub-rect uploads), but now batched to reduce per-glyph
  overhead. This matches current renderer capabilities while keeping update frequency low.
- Glyph eviction bounds the metadata cache size; atlas pages remain allocated as created.

## Suggested Tests
- Load console/menu/centerprint with the default TTF fonts and verify immediate readability.
- Run `cl_font_glyph_cache_size 128` and spam chat with mixed ASCII/CJK to ensure fallback and
  kerning remain correct.
- Compare `Font_MeasureString` sizing before/after kerning on tight UI labels.
