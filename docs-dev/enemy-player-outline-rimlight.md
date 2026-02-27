# Player Rim Light

## Overview
Adds a rim light pass to player models to boost silhouette visibility without
rendering through walls.

## Cvars
`cl_player_rimlight_enemy`

- `0` disables the effect.
- `0.01` to `1.0` enables the effect and controls the rim alpha.
- Default is `0` (off).

`cl_player_rimlight_team`

- `0` disables the effect.
- `0.01` to `1.0` enables the effect and controls the rim alpha.
- Also applies to the local player in thirdperson view.
- Default is `0` (off).

Example:
```
set cl_player_rimlight_enemy 0.4
set cl_player_rimlight_team 0.25
set cl_thirdperson 1
```

## Detection
- Uses the same player/team detection rules as `docs-dev/enemy-player-outline.md`.
- Teammate rim lighting remains depth tested; only outlines render through
  walls.

## Rendering Notes
- Adds a separate model pass flagged `RF_RIMLIGHT` with additive blending.
- The rim term is tinted with the brightskin color selection (custom or
  default).
- Rim alpha is multiplied by per-entity alpha (invisibility or thirdperson
  fade) and the brightskin color alpha.
- When the legacy (non-shader) backend is used, the pass falls back to a flat
  additive overlay in the chosen color.
