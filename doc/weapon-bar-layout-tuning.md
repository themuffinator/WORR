# Weapon Bar Layout Adjustments

## Summary
This update tunes the weapon bar layouts to match the Quake Live/Q3A
references provided and adjusts the static bars to render at half size.

## Static bar size
Modes `1-3` now draw using a 0.5 scale factor:
- Weapon icons and selection highlights are rendered at half size.
- Spacing/padding scales with the icon size to keep the bar compact.
- Ammo counters retain the existing scale logic.

## Bar 3 (static center)
Mode `3` now follows the static tile layout described in
`doc/weapon-bar-static-tiles.md`, using a centered vertical stack with the
icon on the left and value on the right.

## Bar 4 (timed Q3 legacy)
Mode `4` now draws the selected weapon name above the bar, matching the
Q3A `CG_DrawWeaponSelect` behavior (style 1).

## Reference
Aligned with Q3A weapon select logic:
`E:\_SOURCE\_CODE\baseq3a-master\code\cgame\cg_weapons.c`
