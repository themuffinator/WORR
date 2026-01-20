# Player Visibility Controls

## Overview
Adds player model overrides and expanded visibility tuning for brightskins,
outlines, and rim lighting. Enemy and teammate settings are split into
dedicated cvars, and outlines gain a configurable thickness that stays visible
at range.

## Forced Models
`cl_force_enemy_model`

- Forces opponent player models to the given `model/skin` string.
- Example: `set cl_force_enemy_model male/grunt`.

`cl_force_team_model`

- Forces teammate player models to the given `model/skin` string.
- Example: `set cl_force_team_model female/athena`.

Notes:
- Values use the same model/skin syntax as `skin` userinfo.
- Use an empty string or `0` to disable the override.
- For spectators, forced models are ignored.
- Force overrides do not apply to `RF_CUSTOMSKIN` or disguise skins.

## Outlines and Rim Lighting
`cl_player_outline_enemy` / `cl_player_outline_team`

- `0` disables the outline; `0.01` to `1.0` sets outline alpha.
- Team settings also apply to the local player in thirdperson view.

`cl_player_rimlight_enemy` / `cl_player_rimlight_team`

- `0` disables the rim light; `0.01` to `1.0` sets rim alpha.
- Team settings also apply to the local player in thirdperson view.

`cl_player_outline_width`

- Outline thickness in pixels, clamped to `0.5` to `6.0`.
- The renderer scales the outline to keep a minimum screen thickness at
  distance, with a base scale of `1.02` and a maximum scale of `3.0`.

Color Notes:
- Outline and rim colors follow the brightskin selection.
- When custom brightskin colors are enabled, outlines/rims use those values.
- Otherwise, they use the default mapping (free=green, team1=red, team2=blue).

## Spectator Behavior
- Custom brightskin colors are ignored while spectating; defaults are used.
- Forced models do not apply to spectators.

## Legacy Migration
On startup, legacy cvars are migrated when the new ones are unset:
- `cl_enemy_outline` -> `cl_player_outline_enemy` and `cl_player_outline_team`
- `cl_enemy_outline_self` -> `cl_player_outline_team`
- `cl_enemy_rimlight` -> `cl_player_rimlight_enemy` and `cl_player_rimlight_team`
- `cl_enemy_rimlight_self` -> `cl_player_rimlight_team`
