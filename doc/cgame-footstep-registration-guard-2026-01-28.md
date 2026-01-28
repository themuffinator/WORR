# CGame footstep registration guard (2026-01-28)

## Summary
Added safety checks in cgame footstep registration to prevent crashes when footstep material IDs are out of range or when material metadata is unavailable at registration time.

## Details
- Validate the material count returned by `BSP_LoadMaterials` and bail with a warning if it is invalid.
- Guard the footstep array allocation and return cleanly if allocation fails.
- Skip texinfo entries whose `step_id` falls outside the allocated footstep array range.

These checks prevent an access violation in `CL_RegisterFootsteps` when an invalid `step_id` is encountered while iterating `cl.bsp->texinfo`.

## Files touched
- `src/game/cgame/cg_tent.cpp`

## Build note
- `ninja -C builddir` links `cgamex86_64.dll`, but the post-link copy step failed due to a permissions error writing to `builddir/baseq2/cgamex86_64.dll`.
