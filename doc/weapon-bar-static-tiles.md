# Weapon Bar Static Tile Layout

## Summary
Static weapon bar modes (1, 2, 3) now render as horizontal tiles rather
than standalone icons. Each tile includes the weapon icon and its value
in a single row, with a grey background behind both elements.

## Layout rules
- Mode `1` (static left): icon on the left, value to the right.
- Mode `2` (static right): icon on the left, value aligned to the right edge.
- Mode `3` (static center): horizontal tiles centered on the HUD.
  Bars 1/2 align to the safe-zone edges (no extra margin).

## Visual treatment
- Tiles are drawn at half scale to satisfy the static bar size requirement.
- A grey rectangle (RGBA 96,96,96,160) covers icon + value for the selected
  weapon only.
- Values use scaled character drawing so they shrink with the tiles.
