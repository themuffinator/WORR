# Projectile Loop Attenuation Parse Fix (2026-02-27)

Task reference: `FR-06-T01`

## Summary
- Fixed client q2proto entity parsing so loop sound extension fields are actually applied:
  - `Q2P_ESD_LOOP_VOLUME`
  - `Q2P_ESD_LOOP_ATTENUATION`
- This resolves stale loop attenuation states that could leave projectile loop sounds at level-wide/full-volume behavior.

## Root Cause
- `src/client/parse.cpp` (`apply_entity_delta`) handled base `sound` updates but did not apply loop extension delta bits.
- When an entity number was reused, the client could keep stale `loop_attenuation` from prior usage (including `ATTN_LOOP_NONE` / `-1`), causing `dist_mult == 0` and full-volume spatial behavior.

## Code Change
- File: `src/client/parse.cpp`
- Function: `apply_entity_delta(...)`
- Added:
  - `to->loop_volume = delta_state->loop_volume / 255.f;`
  - `to->loop_attenuation = q2proto_sound_decode_loop_attenuation(delta_state->loop_attenuation);`

## Validation
- Build completed successfully:
  - `meson compile -C builddir-client-cpp20`

