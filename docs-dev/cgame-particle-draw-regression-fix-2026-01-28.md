# Particle draw regression fix (cgame)

Date: 2026-01-28

## Summary
- Restored cgame-side particle/dlight/lightstyle list initialization during InitEffects so particles are allocated and rendered after cgame load.

## Details
- cg_effects.cpp
  - CL_InitEffects now clears light styles, particles, and dlights at cgame init. This matches the need to initialize cgame-owned lists when CL_ClearEffects ran before the cgame module was loaded.

## Notes
- This is a cgame module lifecycle fix; it does not alter legacy protocol or demo behavior.
