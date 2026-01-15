# Wheel Reference UI Alignment

## Summary
- Selection arrow now points in the hovered slot direction (cardinal glyphs).
- Added drop hints showing the current bindings for weapon/ammo drops.

## Directional Arrow
- The arrow glyph is chosen based on the selected slot's direction:
  - Horizontal dominance: `>` or `<`
  - Vertical dominance: `v` or `^`
- This mirrors the rerelease pointer that faces the selected slot.

## Drop Hints
- Drawn only on the weapon wheel (not the inventory/powerup wheel).
- Uses key bindings for `cl_weapnext` and `cl_weapprev`:
  - `[<binding>] Drop Weapon`
  - `[<binding>] Drop Ammo`
- Positioned relative to the wheel center and fades with the wheel alpha.
