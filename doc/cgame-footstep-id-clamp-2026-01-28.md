# CGame footstep ID clamp (2026-01-28)

## Summary
Prevented a fatal assertion in `CL_PlayFootstepSfx` by clamping invalid step IDs to the default footstep set.

## Details
- If a step ID is out of range, it now falls back to `FOOTSTEP_ID_DEFAULT` instead of asserting.
- This preserves legacy behavior while avoiding a crash when BSP materials supply unexpected IDs.

## Files touched
- `src/game/cgame/cg_tent.cpp`

## Build result
- `ninja -C builddir` completes successfully (existing Com_* macro warnings remain).
