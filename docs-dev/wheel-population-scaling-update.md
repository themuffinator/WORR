# Wheel Population + Scaling Update

## Summary
- Weapon and inventory wheels now list only items currently carried.
- Slot lists rebuild while the wheel is open, preserving selection and popout animation by item index.
- `ww_size` default updated to match the 700x700 @ 2560x1440 reference in the current virtual HUD space.

## Population Rules
- Weapons: include only indices returned by `GetOwnedWeaponWheelWeapons`.
- Inventory items: include only entries with `GetPowerupWheelCount(...) > 0`.
- Slots remain sorted by `sort_id`; selection is re-mapped to the same item index when possible.

## Scaling Notes
- Target size is 700x700 on a 1440p canvas (per spec).
- In the 16:9 virtual HUD space (640x360), that maps to 175 units.
- `ww_size` defaults to `175.0` to preserve the native size; smaller values still yield a larger wheel.
