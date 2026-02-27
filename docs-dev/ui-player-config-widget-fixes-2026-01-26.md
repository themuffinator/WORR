# UI Player Config and Widget Fixes (2026-01-26)

## Summary
- Player configuration preview now switches weapons during the switch animation and renders a larger, right-column preview.
- Input field sizing for TTF/OTF now uses UI font metrics to avoid clipped leading characters.
- Dropdown list text now chooses a high-contrast color against list backgrounds.
- Server list parsing now falls back to plain text when a binary master list looks like text.
- Crosshair image list previews use 16x16 tiles in the JSON UI.

## Details
### Player preview
- Added a switch stage using `FRAME_pain301` to `FRAME_pain304` so weapon changes align with the same lower/raise transition used in gameplay.
- Weapon selection remains randomized at each switch stage; weapon entity stays frame-synced with the player entity.
- Preview framing uses a closer origin and a tighter FOV to make the model larger in the right-side column.

### Text input fields
- Input field box width now scales by the measured width of an M glyph (fallback to `CONCHAR_WIDTH`) so proportional fonts do not under-size the control.
- The same sizing logic is used across field layout, mouse hit tests, and draw to keep cursor and selection alignment consistent.

### Dropdown list readability
- Dropdown list value text and overlay rows now compute text color based on background luminance to avoid low-contrast combinations.

### Server list parsing
- Binary list parsing now checks for text-like responses (or non-multiple chunk sizes) and falls back to plain parsing for q2servers.com-style feeds.

## Files touched
- `src/game/cgame/ui/ui_page_player.cpp`
- `src/game/cgame/ui/ui_widgets.cpp`
- `src/game/cgame/ui/ui_page_servers.cpp`
- `src/game/cgame/ui/worr.json`
