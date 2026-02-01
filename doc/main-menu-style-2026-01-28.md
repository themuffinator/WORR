# Main menu style refresh (2026-01-28)

## Overview
- Switch the main menu to Q2R-style rows using gfx/menu_bg_off/on with runtime labels.
- Keep the m_cursor$ animated cursor for focused buttons.
- Align headings/content to the bitmap column and show the player name at top-right.
- Drive UI font selection from scr_font defaults via JSON globals.

## UI JSON changes
- globals.font now references "$scr_font" to match the scr_font default face.
- main menu additions:
  - playerName: true (draws the current "name" cvar at top-right)
  - alignToBitmaps: true (aligns headings/actions with the bitmap column)
  - heading widget for "Game"
  - button widgets for Single Player, Multiplayer, Options, Video, Quit

## Code changes
- Added MenuButtonWidget (BitmapWidget-derived) to draw menu_bg_off/on with a label overlay.
  - Optional textOffset/textSize JSON fields for fine-tuning.
  - Default label size scales with the button image height when not specified.
- Menu now supports player name rendering and bitmap-aligned layout via new flags.
- ui_json parser now recognizes menu-level playerName/alignToBitmaps and item type "button".

## Notes
- Button text uses UI font color sets (normal/active) and is centered vertically in the background image.
- Background images are referenced as gfx/menu_bg_off and gfx/menu_bg_on.
