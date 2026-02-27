# Wheel Hover Scale + Ammo Count Size

## Summary
- Wheel item icons now animate scale when hovered/selected, peaking at 200%.
- Per-slot weapon ammo counts render smaller for better readability on the ring.

## Hover Scale
- Each slot tracks an `icon_scale` value.
- While selected, the target total scale is `ww_hover_scale` (default `2.0`), with a base icon size of `1.5x`.
- The transition time is controlled by `ww_hover_time` (milliseconds, default `200`).

## Ammo Count Size
- Slot counts for weapon items use a reduced size (`ww_ammo_size * 0.4`) on the ring.
- Central (selected) ammo counts now use status bar numeral sprites.
