# Wheel Hover Transition + Opacity Update

## Summary
- Hover scaling is now controlled by dedicated cvars with a fixed 200ms transition.
- Selected icons target 200% of the original 24x24 size by default.
- The wheel image renders at 80% opacity.

## New Hover CVars
- `ww_hover_scale` (2.0, SERVERINFO|LATCH): target total icon scale when selected.
- `ww_hover_time` (200, SERVERINFO|LATCH): transition time in milliseconds.

## Behavior
- Base ring icons are still `1.5x` by default; hover scaling multiplies to the `ww_hover_scale` total.
- Hover transitions use a constant-speed approach so a full 1.0 → 2.0 scale completes in ~200ms.
- The wheel background (`gfx/weaponwheel.png`) uses `80%` alpha, independent of the overall wheel fade.
