# Cgame UI Mouse Interaction Fixes

## Overview
- Fixed mouse hit testing for centered menu items so hover and clicks work across the full row.
- Mouse clicks now re-hit-test before activation to avoid firing the previously focused widget.
- List pages (servers/demos) update selection on hover.
- Player config page forwards mouse events into its embedded menu.
- Menu cursor renders at a fixed 16x16 size for better visibility.
- Weapon wheel hover scale now honors `ww_hover_scale`.

## Menu hit testing
- Center-aligned `ActionWidget`/`FieldWidget` layouts now place the hit rectangle around the full menu row.
- Draw alignment is decoupled from the hit rectangle so centered text still renders correctly.
- Mouse clicks use a hit test at the current cursor position and only activate when a widget is under the cursor.

## List hover
- `ListWidget` adds a hover helper that updates `curvalue` and fires `onChange`.
- Server browser and demo browser call this on mouse move to highlight rows under the cursor.

## Cursor sizing
- UI cursor draws with `R_DrawStretchPic` at 16x16 (scaled by UI scale).
- Both default cursor load and JSON cursor overrides clamp the draw size to 16x16.

## Weapon wheel
- `CG_Wheel_GetHoverScale` now uses `ww_hover_scale` (clamped to non-negative values).
