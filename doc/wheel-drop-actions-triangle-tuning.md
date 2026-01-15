# Wheel Drop Actions + Triangle Tuning

## Drop Actions
- Ammo drop now issues `drop "<ammo name>"` using the weapon's ammo item name
  (from the wheel ammo table), instead of `drop ammo`.
- Drop commands now localize `$item_*` tokens into explicit pickup names before
  sending the command, preventing `unknown item` errors.
- Ammo drop is disabled for weapons without ammo (no ammo index, zero count, or
  missing ammo item name).
- Weapon drop is disabled for "ammo weapons" where the weapon item matches the
  ammo item (eg. grenades/tesla), and for any weapon flagged `can_drop = false`.

## Drop Hint Visibility
- Drop hints now render only for the currently selected wheel slot.
- Each hint is shown only when the matching action is valid for that slot.

## Pointer Triangle
- Pointer triangle uses `DrawPolygon()` with a 2:1 width/height ratio.
- Gradient runs from white at the tip to `#434343` at the base.
- A 2x2 supersample within `DrawPolygon()` smooths edges.

## Bind Icon Sizing
- Bind icons are now rendered at 3× the font line height.
- The icon is vertically centered relative to its text line.

## Hover Scale
- Hover scale is locked to a 200% peak to match rerelease behavior.

## Input Selection
- `+attack` no longer selects/uses a wheel item; selection only happens on wheel
  close (release) based on the current hover.
