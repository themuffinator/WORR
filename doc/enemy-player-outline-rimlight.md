# Enemy Player Rim Light

## Overview
Adds a rim light pass to enemy player models to boost silhouette visibility
without rendering through walls.

## Cvar
`cl_enemy_rimlight`

- `0` disables the effect.
- `0.01` to `1.0` enables the effect and controls the rim alpha.
- Default is `0` (off).

`cl_enemy_rimlight_self`

- `0` disables the effect.
- `0.01` to `1.0` enables the effect and controls the rim alpha for the local
  player in thirdperson view.
- Default is `0` (off).

## Detection
- Uses the same player/team detection rules as `doc/enemy-player-outline.md`.
- In teamplay, allies receive rim lighting in team colors but remain depth
  tested (only outlines draw through walls).

## Self Testing
- When `cl_enemy_rimlight_self` is non-zero and thirdperson view is active,
  the rim pass also applies to the local player model.
- The self-test alpha uses the dedicated `cl_enemy_rimlight_self` value.

Example:
```
set cl_enemy_rimlight 0.4
set cl_thirdperson 1
set cl_enemy_rimlight_self 0.5
```

## Rendering Notes
- Adds a separate model pass flagged `RF_RIMLIGHT` with additive blending.
- The GLSL mesh shader computes the rim term from the view direction and
  per-vertex normals, then tints it with the team color in teamplay (red
  otherwise).
- The rim alpha is multiplied by any per-entity alpha (invisibility or
  thirdperson fade).
- When the legacy (non-shader) backend is used, the pass falls back to a flat
  additive overlay in the chosen color.
