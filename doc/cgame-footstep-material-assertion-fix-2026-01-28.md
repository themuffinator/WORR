# CGame footstep material assertion fix (2026-01-28)

## Summary
Resolved a fatal assertion in `CL_RegisterFootstep` when BSP materials supply an empty string for the footstep material.

## Details
- Treat empty material strings as `NULL` so the default footstep path is used.
- This avoids the `Q_assert(!material || *material)` failure during `CL_RegisterFootsteps`.

## Files touched
- `src/game/cgame/cg_tent.cpp`

## Build result
- `ninja -C builddir` completes successfully (warnings about Com_* macro redefs remain).
