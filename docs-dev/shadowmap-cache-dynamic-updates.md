# Shadowmap Cache Updates - Dynamic Casters and Projectile Lights

## Overview
- Static shadowmap caches now invalidate when shadow-casting entities move/animate or when a static light's parameters change.
- Weapon projectile and explosion models are excluded from shadowmap casting to avoid self-shadow artifacts.

## Static Light Cache Invalidation
- Added a scene hash built from shadow-casting entities (model handle, origin, angles, scale, frame/oldframe/backlerp).
- When the hash changes, all static shadowmap cache layers are marked dirty so moving/animated models update shadows.
- Static light cache entries now track origin, radius, and spot cone; any change marks the light's cached faces dirty.

## Projectile/Explosion Self-Shadow Avoidance
- Shadowmap pass honors RF_NOSHADOW again; weapon projectile entities already set this via effect flags.
- Explosion models created in `CL_PlainExplosion` and `CL_BFGExplosion` are flagged RF_NOSHADOW so their own lights do not cast self-shadows.

## Notes
- Animated characters will force static shadowmap refreshes while moving/animating.
- Shadowmaps still cast from translucent models, but projectiles/explosions are skipped.

## Manual Check
- Place a static shadowlight near a moving monster or rotating bmodel and verify shadows update as the model moves.
- Trigger a rocket or grenade explosion and confirm the explosion model does not cast into its own light.
