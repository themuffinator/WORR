# Wheel Visual Tuning (Drop Hints + Center Readout)

## Summary
- Drop hint text moved left and slightly higher to better match rerelease placement.
- Ring icons are scaled up ~50% by default, with selection scale driven by `ww_popout_amount`.
- Weapon ring ammo values are reduced to roughly half of their previous size.
- Center ammo counts now render with status bar numerals, and item names sit one line lower.

## Icon Scale
- Base icon scale is `1.5x` (normal items).
- Selected icon scale uses `ww_hover_scale` as the total scale target.
- With default `ww_hover_scale = 2.0`, the selected icon is ~`2.0x` the original 24x24 size.

## Center Readout
- Center counts use status bar digit sprites (`num_*/anum_*`) instead of font text.
- Item names are shifted down by three line heights to align with the rerelease layout.
- Center count height targets 144px at 1440p (10% of canvas height).

## Drop Hints
- Drop hint block starts at the deadzone radius on the X axis.
- Binding labels remain pulled from `cl_weapnext` / `cl_weapprev`.
