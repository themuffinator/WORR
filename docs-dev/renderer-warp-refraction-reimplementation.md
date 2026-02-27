# Warp Refraction Reimplementation

## Goal
- Make refraction visible and consistent across varying UV scales and distances.
- Keep distortion aligned with the same warp pattern used by the surface texture.
- Preserve the existing cvar gate (`gl_warp_refraction`).

## Implementation Summary
- Refraction now converts the warp offset from texture space into screen-space
  pixels using GLSL derivatives (`dFdx`/`dFdy`) of the base UVs.
- The screen-space offset is then applied to the scene copy sample to distort
  what is seen through the transparent warp surface.
- A small determinant check guards against degenerate UV gradients and falls
  back to the direct offset path if needed.

## Shader Flow (Transparent + Warp Surfaces)
1. Compute base UVs (`tc_base = v_tc`).
2. Compute the same warp offset used for texturing (`warp_ofs`).
3. Convert `warp_ofs` to a screen-space pixel offset:
   - `J = [dFdx(tc_base), dFdy(tc_base)]`
   - `screen_px = inverse(J) * (warp_ofs * gl_warp_refraction)`
4. Sample the refraction scene copy at `base_tc + screen_px * inv_screen_size`.
5. Apply the blend-compensated color solve so the final blend matches the
   refracted target.

## Notes
- The refraction path still copies the scene before alpha faces are drawn.
- Derivatives are core in GLSL 130+ and GLSL ES 300.
