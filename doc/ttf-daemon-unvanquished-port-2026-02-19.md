# Daemon/Unvanquished Font Pipeline Port (2026-02-19)

## Goals
- Replace WORR's previous SDL3_ttf + HarfBuzz-centric glyph handling stack.
- Port to a Daemon-engine-style FreeType chunk renderer with deterministic bitmap-space metrics.
- Preserve WORR's existing client font API (`Font_Load`, `Font_DrawString`, `Font_MeasureString`, fallbacks).

## Upstream references used
- `Daemon/src/engine/renderer/tr_font.cpp`:
  - Chunked Unicode rendering (`0x110000 / 256` chunk model).
  - FreeType outline rasterization with explicit glyph box metrics.
  - Atlas-page assembly and lazy glyph generation.
- `Daemon/src/engine/client/cl_scrn.cpp`:
  - Baseline-oriented glyph placement model used by the runtime draw path.
- Unvanquished:
  - Verified usage pattern remains engine-driven font rendering (Unvanquished consumes Daemon font APIs rather than implementing a second glyph rasterizer).

## What changed

### `src/client/font.cpp`
- Replaced the old implementation with a new FreeType-based TTF pipeline.
- Removed runtime dependency on SDL3_ttf layout/metrics and HarfBuzz shaping in this module.
- Implemented Daemon-style TTF internals:
  - Unicode chunk cache (`256` glyphs per chunk).
  - Lazy chunk generation (`font_ttf_render_chunk`) when glyphs are requested.
  - Outline rasterization using FreeType (`FT_Load_Glyph` + `FT_Outline_Get_Bitmap`).
  - Bitmap-space glyph metrics (`left`, `top`, `bottom`, `x_skip`) used directly at draw time.
  - Atlas page packing with fixed padding and `R_RegisterRawImage` uploads.
- Preserved and kept integrated:
  - Legacy font path (`R_DrawStretchChar` fallback).
  - `.kfont` path and fallback chain.
  - UTF-8 decoding and Quake color escape handling.
  - Existing public API signatures in `inc/client/font.h`.
- Kept `font_dump_glyphs` command and rewired it to the new glyph structures.

### `meson.build`
- Added `freetype2` explicitly to `client_deps` when found.
- This ensures direct FreeType headers used by the new `src/client/font.cpp` are always available to the client target.

## Behavioral differences vs previous WORR path
- Glyph positioning now follows a single FreeType-metric model from rasterization through draw.
- Previous mixed metric sources and shaped-index atlas behavior are removed.
- Spacing/kerning now comes from FreeType pair kerning (`FT_Get_Kerning`) only.
- Complex-script shaping previously provided by HarfBuzz in this file is no longer active.

## Validation
- Full build completed successfully:
  - `meson compile -C builddir`
- `worr.exe`, `worr.ded.exe`, render DLLs, and game DLLs all linked.

## Files touched
- `src/client/font.cpp`
- `meson.build`
- `doc/ttf-daemon-unvanquished-port-2026-02-19.md`
