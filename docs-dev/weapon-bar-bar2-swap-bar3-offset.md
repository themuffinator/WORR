# Weapon Bar Bar-2 Swap and Bar-3 Offset

## Summary
This update adjusts static bar layouts:
- Bar 2 swaps the icon and value positions, while keeping the value right aligned.
- Bar 3 shifts the horizontal tile row down by half a tile height.

## Details
- Bar 2 (static right) now places the icon on the right side of the tile and
  the value on the left, with right-aligned text up to the icon padding.
- Bar 3 (static center) uses the same horizontal tile layout as before, but the
  Y position is increased by `tile_height / 2` to match the requested baseline.
