# Shadowmap Light Direction Fix

## Summary
Shadowmap sampling for per-pixel dynamic lights expects a light-to-fragment
vector to choose the correct cubemap face. The previous GLSL used a
fragment-to-light vector for both lighting and shadow sampling, which led
to sampling the opposite cubemap faces and effectively disabling shadows.

## Changes
- Adjusted dynamic light shading to compute the light-to-fragment vector
  for shadow sampling, and used the inverse vector for Lambert lighting.
- This preserves existing lighting behavior while aligning shadow sampling
  with the cubemap face orientation used during the shadowmap render pass.

## Files
- src/rend_gl/shader.c

## Verification Notes
- Run a map with `gl_shadowmaps 1` and confirm point shadow lights and
  dynamic lights cast shadows.
- Toggle `gl_shadowmap_filter` between 0/1/2 to confirm the shadowmap
  texture is sampled correctly across methods.
