# Shadowmap Light Vector Fix

## Summary
Shadowmap sampling used the normal-offset light position that the shading
path uses for point lights. This offset changes the light direction and
distance, which can desync the shadow lookup from the actual shadowmap
render origin. The mismatch prevented shadows from appearing on some
surfaces (notably world geometry and player models).

## Fix
- Split the light vectors in the dynamic light shader:
  - `shadow_vec`/`shadow_dist` use the original light position for
    shadowmap sampling.
  - The shading direction still uses the normal-offset position for the
    existing light falloff/lambert behavior.
- Shadowmap sampling now uses the correct light-to-fragment vector,
  independent of the lighting offset hack.

## Files
- src/rend_gl/shader.c

## Verification Notes
- Load a map with dynamic lights and `gl_shadowmaps 1`.
- Confirm shadows from weapon/projectile lights appear on world geometry
  and player models.
- Confirm lighting falloff behavior remains unchanged for point lights.
