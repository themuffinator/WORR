# Thirdperson Self Rim Light Test

## Overview
The local player model uses the teammate rim lighting settings while in
thirdperson view. This is a simple way to verify the rim pass visually.

## Cvars
`cl_player_rimlight_team`

- `0` disables the effect.
- `0.01` to `1.0` enables the effect and controls the rim alpha.

`cl_thirdperson`

- Must be enabled to see the local player model.

Example:
```
set cl_thirdperson 1
set cl_player_rimlight_team 0.5
```

## Rendering Notes
- Uses the same rim light pass as teammate players.
- Rim alpha is multiplied by per-entity alpha (thirdperson fade or
  invisibility) and the brightskin color alpha.
