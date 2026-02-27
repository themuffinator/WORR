# UI List Styling Pass (2026-01-22)

## Goals
- Make the demo and server browser lists denser and more readable with size-6 text, centered baselines, and minor padding.
- Add alternating row shading for quicker scanning.
- Shift UI accent colors to a grungy green palette across WORR menus.

## Implementation Details
- Added size-specific UI font helpers so lists can render at virtual line height 6, with a fallback to the default UI font handle if loading fails.
- Demo and server browser list widgets now opt into font size 6, row height = text height + 2px, and alternating row shading (0.8x darken factor), with vertical centering based on actual font line height.
- Alternating rows reuse the per-column background color (normal/active) and apply a modest darkening pass before selection highlights.
- UI accent colors updated in menu globals and defaults to: normal #3e643b70, active #5b8a4a90, selection #3f6a3ab0, disabled #7f7f7f.

## Files Touched
- src/client/ui_font.cpp
- inc/client/ui_font.h
- src/game/cgame/ui/ui_list.cpp
- src/game/cgame/ui/ui_internal.h
- src/game/cgame/ui/ui_page_demos.cpp
- src/game/cgame/ui/ui_page_servers.cpp
- src/game/cgame/ui/worr.json
- src/client/ui/worr.menu
- src/game/cgame/ui/ui_core.cpp
- src/client/ui/ui.cpp
