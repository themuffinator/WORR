# Enemy Player Outline

## Overview
Adds an optional stencil outline for player models to improve visibility.
Enemy outlines respect depth; in teamplay, allied outlines use team colors
and render through walls.

## Cvar
`cl_enemy_outline`

- `0` disables the effect.
- Any non-zero value enables the effect.
- In teamplay, this also enables allied outlines (rendered through walls).
- Default is `0` (off).

`cl_enemy_outline_self`

- `0` disables the effect.
- Any non-zero value enables the effect.
- Only applies to the local player while in thirdperson view.
- Default is `0` (off).

`cl_enemy_rimlight`

- `0` disables the rim light pass.
- `0.01` to `1.0` enables the effect and controls the rim alpha.
- Uses the same enemy detection rules as the outline.
- The local player can be included for testing via `cl_enemy_rimlight_self`.
- Default is `0` (off).

`cl_enemy_rimlight_self`

- `0` disables the effect.
- `0.01` to `1.0` enables the effect and controls the rim alpha for the local
  player in thirdperson view.
- Default is `0` (off).

Example:
```
set cl_enemy_outline 1
set cl_enemy_rimlight 0.35
set cl_enemy_rimlight_self 0.35
```

## Enemy Detection
- Re-release protocol: the local `player_state_t::team_id` is compared against
  the packed `player_skinnum_t::team_index` for each player entity.
  - When `team_id` is `0`, all other player entities are treated as enemies.
  - When `team_id` is non-zero, players on a different team are treated as
    enemies; allies receive a separate outline that can render through walls.
  - Players with `team_index` `0` (spectator/unassigned) are ignored in team
    modes.
- Corpse entities (body queue slots) and players flagged as dead POIs are
  excluded from outline and rim effects.
- Non-re-release protocol: all other player entities are treated as enemies
  because team metadata is not available.

## Rendering Notes
- The outline is a stencil-buffer silhouette pass that draws a thin opaque
  line around the model.
- The stencil approach is detailed in `doc/enemy-player-outline-stencil.md`.
- The outline pass requires a stencil buffer and is skipped when unavailable.
- The optional rim light pass (`RF_RIMLIGHT`) uses additive blending and is
  described in `doc/enemy-player-outline-rimlight.md`.
- Outline alpha is multiplied by the per-entity alpha (invisibility or
  thirdperson fade). Teamplay outlines use the target player's team color.
- In teamplay, rim lights also use team colors but do not render through walls.
- Teamplay visibility and PVS behavior are detailed in
  `doc/enemy-player-outline-teamplay.md`.
