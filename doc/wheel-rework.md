# Wheel Rework (Weapon + Inventory)

## Overview
- Rebuilt weapon/inventory wheel layout, input, and selection to match the 768x768 hoop design and the 700x700 @ 2560x1440 reference.
- Added popout animation, unavailable shading, cursor clamping, and deadzone retention timing.
- Updated action bindings so +attack or wheel release confirms, while cl_weapprev/cl_weapnext trigger drop actions when the wheel is open.
- Wheel slots now list only items currently carried.

## Geometry + Scaling
- Wheel image size: 768x768; ring radius 256; deadzone radius 144; item radius 200; cursor clamp radius 256.
- The draw size is based on 700x700 at 2560x1440 and scaled by `ww_size` (smaller value = larger wheel).
- Screen position is derived from `ww_screen_frac_x`/`ww_screen_frac_y`; inventory wheel mirrors the X offset to the left.

## Input + Selection
- On open, the mouse cursor warps to the wheel center (frac X/Y) and starts at radius 0.
- Cursor motion is scaled by `ww_mouse_sensitivity`, with `ww_mouse_deadzone_speed` applied inside the deadzone.
- Selection is only valid in the 144..256 radius ring; entering the deadzone keeps the last selection for `ww_deadzone_timeout` ms.
- `ww_controller_exit_timeout` blocks wheel input briefly after closing to avoid immediate re-input.

## Actions
- Use: +attack or wheel release calls `use_index_only` on the selected item.
- Ammo drop: `cl_weapprev` issues `drop ammo` when a weapon is selected.
- Weapon drop: `cl_weapnext` issues `drop "<item name>"` when a droppable weapon is selected.
- When the wheel is open, cl_weapprev/cl_weapnext are consumed by the wheel (no carousel cycling).

## Visuals
- Items are laid out clockwise from the top, evenly spaced by slot count.
- Selected icons pop out by `ww_popout_amount` with `ww_popout_speed` easing.
- Unavailable items are shaded using `ww_unavailable_shade_value` (rare now that unowned items are not listed).
- Selection arrow uses a caret glyph positioned by `ww_arrow_offset`.
- A subtle under-icon nudge uses `ww_underpic_nudge_amount` to offset the selected underlay.

## New/Updated CVars
- `ww_ammo_size` (24.0, SERVERINFO|LATCH)
- `ww_arrow_offset` (102.0, SERVERINFO|LATCH)
- `ww_controller_exit_timeout` (150, USERINFO|LATCH)
- `ww_deadzone_timeout` (350, USERINFO|LATCH)
- `ww_hover_scale` (2.0, SERVERINFO|LATCH)
- `ww_hover_time` (200, SERVERINFO|LATCH)
- `ww_mouse_deadzone_speed` (0.5, SERVERINFO|LATCH)
- `ww_mouse_sensitivity` (0.75, SERVERINFO|LATCH)
- `ww_popout_amount` (4.0, SERVERINFO|LATCH)
- `ww_popout_speed` (7.2, SERVERINFO|LATCH)
- `ww_screen_frac_x` (0.76, SERVERINFO|LATCH)
- `ww_screen_frac_y` (0.5, SERVERINFO|LATCH)
- `ww_size` (175.0, SERVERINFO|LATCH)
- `ww_timer_speed` (3.0, SERVERINFO|LATCH)
- `ww_unavailable_shade_value` (80, USERINFO|LATCH)
- `ww_underpic_nudge_amount` (4.0, SERVERINFO|LATCH)
