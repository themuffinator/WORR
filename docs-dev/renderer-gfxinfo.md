# GL gfxinfo command

## Why
Debugging postprocess/HDR features needs a quick way to verify which renderer
is active and what GL capabilities/extensions are available. Quake3 shipped a
`gfxinfo` command for this purpose, so WORR now provides a similar command.

## Usage
- `gfxinfo`: prints vendor/renderer/version, limits, and capability flags.
- `gfxinfo 1`: also dumps the full extension list.

## Output highlights
- Active renderer + backend type (GLSL vs legacy).
- GL vendor/renderer/version + GLSL version (when available).
- Texture limits and draw buffer counts.
- Pixel format bits and anisotropy support.
- Postprocess status (shader backend, HDR active, framebuffer OK).

## Files
- `src/rend_gl/main.c`: adds the `gfxinfo` console command.
