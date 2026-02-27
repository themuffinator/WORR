# Font DrawString Constness Update (2026-01-20)

## Summary
`Font_DrawString` now accepts a non-const `font_t *` to avoid mutating a
const object during glyph caching and atlas updates.

## Rationale
The draw path updates TTF glyph caches and atlas pages, which are inherently
mutable. Accepting a non-const pointer makes this explicit and removes the
need for `const_cast`.

## Files Touched
- `inc/client/font.h`
- `src/client/font.cpp`
