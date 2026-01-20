# Player Shell Rim Light

## Overview
Adds a rim light pass to player models and their carried weapon models when a
color shell is active, matching the shell color.

## Cvar
`cl_player_rimlight_shell`

- `0` disables the shell rim light pass.
- `0.01` to `1.0` enables the pass and scales rim alpha.
- Default is `1` (on).
- Archived.

Example:
```
set cl_player_rimlight_shell 0
```

## Color Matching
- Uses the same shell color mapping as the renderer:
  - `RF_SHELL_LITE_GREEN` -> (0.56, 0.93, 0.56)
  - `RF_SHELL_HALF_DAM` -> (0.56, 0.59, 0.45)
  - `RF_SHELL_DOUBLE` -> (0.9, 0.7, 0.0)
  - Red/green/blue shell bits force those channels to `1.0`.
- Rogue pack shell mixing runs first so rim colors match the mixed shell.

## Rendering Notes
- Adds a separate `RF_RIMLIGHT` pass with alpha scaled to
  `entity_alpha * 0.30 * cl_player_rimlight_shell` (same base opacity as the
  shell pass).
- Applies to the player model and linked weapon model (`modelindex2`) when
  `EF_COLOR_SHELL` is set.
