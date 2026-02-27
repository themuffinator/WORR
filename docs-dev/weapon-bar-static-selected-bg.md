# Weapon Bar Static Selection Background

## Summary
Static weapon bar modes now draw the grey background tile only for the
currently selected weapon. Non-selected weapons render without a tile
background.

## Affected modes
- `cl_weapon_bar 1` (static left)
- `cl_weapon_bar 2` (static right)
- `cl_weapon_bar 3` (static center, horizontal tiles)

## Details
- Background color: RGBA 96,96,96,160.
- Selection highlight remains on the selected icon.
- Horizontal tiling for mode 3 uses the same tile style as modes 1/2.
