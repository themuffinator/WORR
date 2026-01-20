# Thirdperson Self Outline Test

## Overview
The local player model uses the teammate outline and rim lighting settings
while in thirdperson view. This makes it easy to verify the effects without
joining a team match.

## Cvars
`cl_player_outline_team`

- `0` disables the outline.
- `0.01` to `1.0` enables the effect and controls outline alpha.

`cl_player_rimlight_team`

- `0` disables the rim light pass.
- `0.01` to `1.0` enables the effect and controls rim alpha.

`cl_thirdperson`

- Must be enabled to see the local player model.

Example:
```
set cl_thirdperson 1
set cl_player_outline_team 0.35
set cl_player_rimlight_team 0.35
```
