# CRT scanline visibility fix

## Problem
The CRT pass still looked like a simple brightening effect because the scanline
modulation was too subtle at native resolution. With hardScan at -8.0, the
previous modulation only darkened alternating lines to ~43% intensity, which was
not visually distinct against the game’s lighting and post effects.

## Fix
- Strengthened scanline modulation by mapping `r_crt_hardScan` to a deeper
  alternating-line attenuation using `exp2(hardScan * 0.25)`.
- Added `crt_params2.w` as a scanline scale factor, computed from
  `r_config.height / glr.render_height`, to keep scanline spacing aligned when
  resolution scaling is active.

## Files
- `src/rend_gl/main.c`
- `src/rend_gl/shader.c`
