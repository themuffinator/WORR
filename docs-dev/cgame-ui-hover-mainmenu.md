# Cgame UI Hover and Main Menu Layout

## Summary
- Hover/focus text now visibly changes color by avoiding `UI_ALTCOLOR` on focused/disabled items.
- Main menu bitmap buttons align left within a centered column, matching the legacy Quake II menu layout.

## Hover visibility
- `UI_ALTCOLOR` forces the alternate font glyphs and overrides the draw color to white.
- The menu widgets now only use `UI_ALTCOLOR` when unfocused and enabled, allowing focused text to render with `uis.color.active`.
- Affected widgets: action, slider, spin, field labels, keybind, save/load slots.

## Main menu bitmap alignment
- When bitmap items exist, the menu computes a left-aligned column that is centered as a group with the plaque/logo.
- The column left edge is derived from `widest_bitmap + CURSOR_WIDTH + max(plaque, logo)` so the full group stays centered.
- Plaque/logo positions are anchored relative to the bitmap column left edge, matching the original menu layout.
