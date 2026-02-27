# Shadowmap Shadow Casting Revision

## Goals
- Ensure shadow maps include all opaque world geometry and models within each light's range.
- Avoid area-portal culling during shadow passes that removed occluders for moving lights.
- Reduce shadow map near-plane clipping for close-range casters.
- Improve default shadowmap filtering quality.
- Prevent projectile/explosion models from self-shadowing their own lights.

## Implementation Details
- Shadow passes now ignore `refdef.areabits` so world geometry is evaluated solely by light PVS/frustum.
- Shadow caster draws use a dedicated pass over `refdef.entities` with range culling based on light radius plus model/bmodel bounds.
- Shadow map near plane is reduced for smaller lights (1% of radius, min 1.0) to keep near geometry in the map.
- Default shadowmap settings updated for higher-quality filtering and a higher light count:
  - `gl_shadowmap_lights` = 4
  - `gl_shadowmap_filter` = 2 (VCM)
  - `gl_shadowmap_quality` = 3
  - `gl_shadowmap_bias` = 0.003
  - `gl_shadowmap_softness` = 1.0

## Projectile/Explosion Self-Shadowing
- Added `RF_NOSHADOW` to weapon-impact and explosion effect models that emit lights to avoid self-shadow artifacts.

## Files Touched
- `src/rend_gl/main.c`
- `src/client/tent.cpp`
