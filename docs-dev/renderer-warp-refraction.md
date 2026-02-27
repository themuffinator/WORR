# Warp Refraction for Transparent Warp Volumes

## Why
Quake II Rerelease uses a gentle refraction on translucent water volumes, but WORR
only warped the surface texture. This adds a subtle, warp-aligned refractive pass
that can be tuned or disabled via a cvar.

## What changed
- Added `gl_warp_refraction` (float, default `0.1`) to scale the refraction offset;
  `0` disables the effect (clamped to `0..2` in shaders for safety).
- Added a GLSL refraction path for `SURF_WARP` + `SURF_TRANS33/66` surfaces:
  - Copies the current scene into a refraction texture before drawing alpha faces.
  - Uses the same warp sine offset to perturb screen UVs.
  - Samples both base and refracted scene colors and adjusts output so standard
    alpha blending produces the intended composite.
- Added a refraction texture slot and binding path in the renderer.
- Refraction now forces an offscreen scene pass (like bloom/warp) when enabled
  so the source texture is always valid.

## Notes
- Refraction is GLSL-only; the legacy backend remains unchanged.
- The scene copy runs once per frame only when a visible transparent warp face is
  present and `gl_warp_refraction` is greater than zero.
- 3D passes now populate `u_crt_texel` so refraction can compute screen UV scale.
- If refraction is the only post effect, the scene is still resolved back to the
  main framebuffer to avoid losing output.

## Files
- `src/rend_gl/gl.h`
- `src/rend_gl/main.c`
- `src/rend_gl/shader.c`
- `src/rend_gl/texture.c`
- `src/rend_gl/tess.c`
- `src/rend_gl/qgl.h`
- `src/rend_gl/qgl.c`
