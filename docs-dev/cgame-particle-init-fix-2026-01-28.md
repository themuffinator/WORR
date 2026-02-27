# CGame particle init fix (2026-01-28)

## Summary
Ensured cgame particle/dlight/lightstyle lists are initialized even if `CL_ClearEffects` ran before cgame entity hooks were installed.

## Details
- `CL_InitEffects` in cgame now calls:
  - `CL_ClearLightStyles()`
  - `CL_ClearParticles()`
  - `CL_ClearDlights()`

This initializes the free particle list so `CL_AllocParticle` can succeed and particles are emitted when cgame takes over entity effects.

## Files touched
- `src/game/cgame/cg_effects.cpp`

## Build result
- `ninja -C builddir` completes successfully (existing Com_* macro warnings remain).
