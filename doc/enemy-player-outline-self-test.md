# Thirdperson Self Outline Test

## Overview
Adds a testing cvar to apply the enemy outline effect to the local player
model while in thirdperson view.

## Cvar
`cl_enemy_outline_self`

- `0` disables the effect.
- Any non-zero value enables the effect.
- Only active when `cl_thirdperson` is enabled and the thirdperson view is in
  use.
- Default is `0` (off).

Example:
```
set cl_thirdperson 1
set cl_enemy_outline_self 0.35
```

## Rendering Notes
- Uses the same stencil-buffer outline pass as enemy outlines.
- The outline inherits per-entity alpha (thirdperson fade or invisibility).
- In teamplay, the outline uses the local player's team color.
- When `cl_enemy_rimlight_self` is enabled, the rim pass also applies to the
  local player with its own alpha control.
