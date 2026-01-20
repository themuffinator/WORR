# Player Third-Person Muzzle Flashes

## Overview
Player weapon muzzle flash models now render in third-person views, matching the
same per-weapon flash type, offsets, and scale used for the first-person
re-release effects. This applies to other players and to the local player when
`cl_thirdperson` is enabled.

## Cvar
`cl_muzzleflashes` controls whether muzzle flash models are spawned.

- `0` disables muzzle flash models (first- and third-person).
- `1` enables muzzle flash models.
- Dynamic light timing still uses `cl_muzzlelight_time`.

## Placement Details
- Remote players on the rerelease protocol use the packed `viewheight` from
  `player_skinnum_t` to reconstruct an eye-level origin before projecting the
  muzzle offset along the model angles.
- The local third-person player uses the predicted player origin plus
  `ps.viewoffset[2] + ps.pmove.viewheight` for the same projection, keeping the
  flash aligned with the rendered model.
- Non-extended protocols fall back to the local viewheight when a packed value
  is not available.
