# Player Stencil Outline

## Overview
Implements a thin stencil outline around player models. Enemy outlines respect
depth; teammate outlines can render through walls.

## Cvars
- `cl_player_outline_enemy` enables enemy outlines (alpha controls strength).
- `cl_player_outline_team` enables teammate outlines (alpha controls strength).
- `cl_player_outline_width` sets outline thickness in pixels.

## Render Flow
- The base player model renders with stencil writes enabled, setting a stencil
  reference value of `1` wherever the model passes the depth test.
- A second pass draws a slightly scaled version of the model in the outline
  color with depth testing enabled and a stencil test of `!= 1`.
- For teammate outlines, an extra stencil-only pass runs with depth testing
  disabled so the outline can render through walls.
- A final color-masked pass clears the model's stencil footprint back to `0`
  so it does not affect other outline draws.

## Outline Thickness
- The outline scale is derived from `cl_player_outline_width` (pixels) and
  the current view distance so thin outlines stay visible at range.
- The renderer clamps `cl_player_outline_width` to `0.5` to `6.0` and enforces
  a minimum scale of `1.02` with a maximum scale of `3.0`.

## Stencil Requirements
- Requires a stencil buffer (`gl_config.stencilbits > 0`).
- When no stencil buffer is available, the outline pass is skipped.

## Notes
- The renderer flag `RF_OUTLINE` marks entities that should receive the
  stencil outline pass.
