# Player Outline and Rim Light

## Overview
Adds optional stencil outlines and rim lighting to player models to improve
visibility. Enemy and teammate effects are controlled separately; teammate
outlines can render through walls. Colors follow the brightskin selection
(custom or default).

## Cvars
`cl_player_outline_enemy`

- `0` disables the outline.
- `0.01` to `1.0` enables the effect and sets outline alpha.
- Default is `0` (off).

`cl_player_outline_team`

- `0` disables teammate outlines.
- `0.01` to `1.0` enables the effect and sets outline alpha.
- Also applies to the local player in thirdperson view.
- Default is `0` (off).

`cl_player_outline_width`

- `0.5` to `6.0` sets outline thickness in pixels (renderer clamps).
- Default is `2.0`.

`cl_player_rimlight_enemy`

- `0` disables the rim light pass.
- `0.01` to `1.0` enables the effect and sets rim alpha.
- Default is `0` (off).

`cl_player_rimlight_team`

- `0` disables teammate rim lighting.
- `0.01` to `1.0` enables the effect and sets rim alpha.
- Also applies to the local player in thirdperson view.
- Default is `0` (off).

Legacy cvars: `cl_enemy_outline`, `cl_enemy_outline_self`, `cl_enemy_rimlight`,
`cl_enemy_rimlight_self` are migrated to the new names on startup.

Example:
```
set cl_player_outline_enemy 0.75
set cl_player_outline_team 0.5
set cl_player_outline_width 2.5
set cl_player_rimlight_enemy 0.35
```

## Enemy/Team Detection
- Re-release protocol: local `player_state_t::team_id` is compared against
  each entity's packed `player_skinnum_t::team_index`.
  - When `team_id` is `0`, all other player entities are treated as enemies.
  - When `team_id` is non-zero, players on a different team are enemies and
    same-team players are allies.
  - Players with `team_index` `0` (spectator/unassigned) are ignored in team modes.
- Corpse entities (body queue slots) and dead POI markers are excluded.
- Non-re-release protocol: all other player entities are treated as enemies
  because team metadata is not available.

## Rendering Notes
- The outline is a stencil-buffer silhouette pass (`RF_OUTLINE`); teammate
  outlines use `RF_OUTLINE_NODEPTH` to render through walls.
- Outline and rim colors use the brightskin selection (custom colors when
  enabled, otherwise red/blue/green defaults).
- Outline alpha and rim alpha are multiplied by per-entity alpha and the
  color alpha.
- Rim lighting uses an additive `RF_RIMLIGHT` pass and remains depth tested.
