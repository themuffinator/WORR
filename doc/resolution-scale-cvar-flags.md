# Resolution scale cvar flags

## Overview
Removed CVAR_LATCH from the GL resolution scaling cvars so adjustments apply
immediately without requiring a restart or renderer reload.

## Changes
- `r_resolutionscale`
- `r_resolutionscale_aggressive`
- `r_resolutionscale_fixedscale_h`
- `r_resolutionscale_fixedscale_w`
- `r_resolutionscale_gooddrawtime`
- `r_resolutionscale_increasespeed`
- `r_resolutionscale_lowerspeed`
- `r_resolutionscale_numframesbeforelowering`
- `r_resolutionscale_numframesbeforeraising`
- `r_resolutionscale_targetdrawtime`

## Files
- `src/rend_gl/main.c`
