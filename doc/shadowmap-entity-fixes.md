# Shadowmap entity fixes

## Overview
These changes make shadow-casting map lights reliably networked, prevent
self-shadow artifacts from projectile/explosion models, and ensure the
shadowmap path uses per-pixel lighting when shadowmaps are enabled.

## Server-side shadowlight spawn
- `setup_dynamic_light` now guards against `MAX_SHADOW_LIGHTS` overflow and
  logs a warning when the limit is hit.
- Shadowlight entities always retain `RF_CASTSHADOW` via bitwise OR and clear
  `SVF_NOCLIENT` so they are not suppressed from networking.
- The light entity is still linked with a zero-size bounds for PHS/PVS.

Files: `src/game/sgame/g_misc.cpp`.

## Shadowmap pass entity suppression
- Shadowmap rendering now skips entities marked `RF_NOSHADOW`, in addition to
  weapon models, so transient effects and other explicitly flagged entities do
  not cast into the shadowmap.

Files: `src/rend_gl/main.c`.

## Projectile/explosion self-shadow avoidance
- Client packet entities with missile-like effects (rocket, blaster, hyperblaster,
  BFG, trap, tracker, ion ripper, plasma, grenade light) are flagged with
  `RF_NOSHADOW` so their own dynamic lights do not self-shadow their models.
- Explosion temp entities already set `RF_NOSHADOW`; the renderer now honors it
  during shadowmap generation.

Files: `src/client/entities.c`, `src/client/tent.c`, `src/rend_gl/main.c`.

## Per-pixel lighting requirement for shadowmaps
- The shader backend reports per-pixel lighting support whenever `gl_shadowmaps`
  is enabled, ensuring world geometry runs the dynamic-light path that applies
  shadowmaps even if `gl_per_pixel_lighting` is off.

Files: `src/rend_gl/shader.c`.
