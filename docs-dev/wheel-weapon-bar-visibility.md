# Wheel UI Visibility + Deadzone Name Offset

## Summary
- Weapon bar drawing is suppressed while the weapon/inventory wheel is active, including the close fade, to prevent overlapping UI.
- Weapon wheel item names shown while the cursor is in the deadzone are lifted by one scaled text height.

## Weapon Bar Suppression
- `CG_WeaponBar_Draw` returns early when the wheel state is not closed or the wheel timer is still non-zero.
- This applies across all weapon bar modes so the bar stays hidden any time the wheel is visible.

## Deadzone Name Offset
- When the wheel is in weapon mode and the cursor is inside the deadzone radius, the selected item name shifts up by the current text height.
- Powerup/inventory wheel name placement is unchanged.
