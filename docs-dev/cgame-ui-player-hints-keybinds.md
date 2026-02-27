# Cgame UI Hints, Keybind Widgets, and Player Preview

## Button hint bar
- Added `UI_DrawHintBar` with keycap/icon rendering to show consistent bottom hints in submenus.
- Default hints now include `Esc Back` and `Enter Select`, plus `Bksp Unbind` when keybind widgets are present.
- Menu layout reserves space for the hint bar and status text shifts above it.

## Menu scroll feedback
- Menus that scroll now show a right-side scrollbar matching the list widget style.

## Keybind widget visuals
- Key binding widgets render graphical keycaps or controller/mouse icons when available, with text fallback.
- `KeyBindWidget` keeps the unbind behavior but now displays primary/secondary bindings as icons.
- Key bind pages add labeled separators via JSON to match the rerelease layout style.

## Player setup preview rework
- Player preview uses a stage-driven animation loop (stand/run/attack/crouch/crouch attack/death).
- Weapon models cycle between available player weapon models to demonstrate switching.
- Muzzle flashes are simulated with a short-lived dynamic light during firing frames.
- The player model rotates slowly to show the full silhouette.

## Server/Demo browser parity
- Server and demo browsers now respond to `Esc`/right-click to return to the previous menu.
- Each browser reserves space for the hint bar and displays hints consistent with other submenus.
