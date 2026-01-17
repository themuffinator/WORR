# Thirdperson Self Rim Light Test

## Overview
Adds a testing cvar to apply the enemy rim light effect to the local player
model while in thirdperson view.

## Cvar
`cl_enemy_rimlight_self`

- `0` disables the effect.
- `0.01` to `1.0` enables the effect and controls the rim alpha.
- Only active when `cl_thirdperson` is enabled and the thirdperson view is in
  use.
- Default is `0` (off).

Example:
```
set cl_thirdperson 1
set cl_enemy_rimlight_self 0.5
```

## Rendering Notes
- Uses the same rim light pass as enemy players.
- The rim alpha is multiplied by any per-entity alpha (thirdperson fade or
  invisibility).
