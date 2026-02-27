# GL CRT postprocess revision

## Why
The initial CRT pass reused a Lottes-style beam profile but assumed the input was lower
resolution than the output. When the postprocess input and output were the same size,
`crt_dist()` evaluated to 0.0 for every fragment. That collapsed scanline weighting to
the center line only and made the effect nearly invisible unless `r_crt_shadow_mask`
was enabled; the only obvious change was the bright boost.

## What changed
- Updated the GLS_CRT fragment path to follow the CRT-Lottes sampling pipeline:
  - sRGB <-> linear conversions gated by `r_crt_scale_in_linear_gamma`.
  - `Fetch` now applies `r_crt_brightboost` before linearization so beam weighting
    tracks boosted intensity.
  - Scanline weighting uses the Tri (3-line) blend described by Lottes.
  - Shadow mask patterns match the Lottes reference (mask 1-4).
- Added an output-space scanline modulation step so scanlines remain visible even
  at 1:1 input/output scale (uses `gl_FragCoord.y`, driven by `r_crt_hard_scan`).

## Notes
- `r_crt_shadow_mask` now accepts 0-4, with 0 disabling the mask.
- CRT output remains a final postprocess pass and does not alter the GL bloom/DOF
  pipeline ordering.

## Files
- `src/rend_gl/shader.c`: GLS_CRT shader path reworked to CRT-Lottes sampling and
  output-space scanline modulation.
- `src/rend_gl/main.c`: shadow mask range clamp updated for the extra mask mode.
