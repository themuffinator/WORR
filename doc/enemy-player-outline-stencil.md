# Enemy Player Stencil Outline

## Overview
Implements a thin stencil outline around player models. Enemy outlines respect
depth; allied outlines can render through walls during teamplay.

## Cvars
- `cl_enemy_outline` enables the outline for enemy players.
- `cl_enemy_outline_self` enables the outline for the local player in
  thirdperson view (testing).

## Render Flow
- The base player model renders with stencil writes enabled, setting a stencil
  reference value of `1` wherever the model passes the depth test.
- A second pass draws a slightly scaled version of the model in the outline
  color with depth testing enabled and a stencil test of `!= 1`.
- For allied outlines in teamplay, an extra stencil-only pass runs with depth
  testing disabled so the outline can render through walls.
- A final color-masked pass clears the model's stencil footprint back to `0`
  so it does not affect other outline draws.

## Outline Thickness
- The outline uses a fixed scale factor of `1.02` (about 2 percent larger than
  the base model).
- The line inherits per-entity alpha (invisibility or thirdperson fade).

## Stencil Requirements
- Requires a stencil buffer (`gl_config.stencilbits > 0`).
- When no stencil buffer is available, the outline pass is skipped.

## Notes
- The renderer flag `RF_OUTLINE` marks entities that should receive the
  stencil outline pass.
