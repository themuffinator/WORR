# Particle accuracy alignment (cgame)

Date: 2026-01-28

## Summary
- Aligned cgame temp-entity beam orientation and explosion flags with q2repro behavior.
- Restored shadowlight handling logic to match q2repro (guarded by cvar and per-pixel support).
- Seeded cgame Q_rand from engine Com_SlowRand to avoid deterministic unseeded particle patterns.

## Details
- cg_tent.cpp
  - CL_AddPlayerBeams: moved pitch/yaw calculation to match q2repro ordering, removing a post-offset recompute that altered beam direction.
  - Removed extra RF_NOSHADOW flags on several explosion/flash temp entities to match q2repro visuals (plain explosion, BFG explosion, flash, and selected temp-entity cases).
- cg_effects.cpp
  - CL_AddShadowLights: restored q2repro guard (cl_shadowlights + R_SupportsPerPixelLighting) and current-frame entity usage.
  - CL_InitEffects: seed Q_rand once using Com_SlowRand so cgame effects share engine-seeded randomness rather than an uninitialized PRNG.

## Notes
- No protocol or demo compatibility changes.
- Intended to bring particle and temp-entity visuals closer to q2repro baseline.
