# OpenGL PNG fallback (stb)

## Summary
- Added stb_image-based PNG load/save in the OpenGL renderer when libpng is unavailable.
- Ensured PNG assets (for example `pics/conchars.png`) remain loadable from pak files.

## Root cause
- The OpenGL renderer only registered PNG support when libpng was present.
- Without libpng, `.png` was treated as an unknown extension, the loader tried `.tga`/`.pcx`,
  and the final error surfaced as `Invalid quake path` for required UI assets.

## Fix
- Introduced `USE_STB_PNG` as a fallback build flag when libpng is not found.
- Added the stb implementation unit to the OpenGL renderer build.
- Implemented `IMG_LoadPNG` and `IMG_SavePNG` using stb and enabled PNG in texture format
  search, screenshot commands, and cvars.

## Files
- `meson.build`
- `src/rend_gl/images.c`
- `src/rend_gl/images.h`

## Validation
- Build without libpng and confirm `pics/conchars.png` loads in the OpenGL renderer.
- Verify `screenshotpng` works and writes a valid PNG.
