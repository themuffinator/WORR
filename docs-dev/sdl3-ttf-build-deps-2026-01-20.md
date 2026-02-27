# SDL3_ttf Build Dependency Stabilization (2026-01-20)

## Motivation
- SDL3_ttf was not being enabled because its fallback dependency chain
  pulled in HarfBuzz/GLib/Cairo and failed under wrap conflicts, leaving
  `USE_SDL3_TTF` off and forcing font fallbacks.

## Changes
- Added a top-level `freetype2` dependency with constrained options
  (harfbuzz/brotli/bzip2/png disabled, zlib internal) so SDL3_ttf resolves
  against a minimal FreeType build.
- Disabled fallback wrapping for SDL3_ttf optional deps (harfbuzz/plutosvg)
  to prevent unnecessary subproject pulls when wrap mode is forced.
- Updated the font loader to the SDL3_ttf 3.x API
  (`TTF_FontHasGlyph`, `TTF_GetGlyphMetrics`, `TTF_RenderGlyph_Blended`,
  `TTF_GetFontAscent`, `TTF_GetFontLineSkip`, `SDL_GetError`).

## Expected Outcome
- SDL3_ttf builds from subprojects on Windows without extra dependency
  downloads or wrap conflicts.
- `USE_SDL3_TTF` is enabled, so TTF/OTF fonts load instead of falling back
  to kfont/legacy.

## Build Notes
- Reconfigure and rebuild to pick up the new dependency graph:
  - `meson setup builddir --reconfigure -Dsdl3-ttf=enabled`
  - `meson compile -C builddir`
