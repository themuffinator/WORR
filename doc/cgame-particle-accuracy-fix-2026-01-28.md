# CGame particle accuracy fix (2026-01-28)

## Summary
Improved particle accuracy by making cgame particle emission obey the renderer particle budget exactly like the original client path.

## Root cause
The cgame port used a local `num_particles` counter in `CL_AddParticles`, which ignored the renderer’s current particle count. This could cause mismatches in which particles were emitted or dropped compared to the original client code that checks `r_numparticles` directly.

## Fix
- Changed `V_AddParticle` to return `bool` indicating whether a particle was accepted.
- Updated cgame `CL_AddParticles` to break immediately when `V_AddParticle` fails, mirroring the original early-out behavior when the particle list is full.
- Updated the cgame import signature and client prototype accordingly.

## Files touched
- `src/client/view.cpp`
- `src/client/client.h`
- `inc/client/cgame_entity.h`
- `src/game/cgame/cg_effects.cpp`
- `src/client/cgame.cpp`

## Build result
- `ninja -C builddir` completes successfully.
