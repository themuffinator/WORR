# Menu Rework Proposal Alignment (JSON UI)

## Overview
This document records the updates applied to `doc/proposals/menu_rework.md` to align the proposal with the current cgame JSON UI system and expand its coverage. The goal is to ensure the proposal targets the actual UI architecture in the repo and enumerates missing widget support and settings coverage in a concrete way.

## Key Updates

### 1) Architecture alignment
* The proposal now targets `worr.json` under `src/game/cgame/ui/` rather than the legacy `worr.menu` script system.
* Implementation steps reference JSON parsing/layout (`ui_json.cpp`, `ui_menu.cpp`, `ui_widgets.cpp`) and the existing condition engine.

### 2) Widget-to-JSON mapping
* Added a mapping section that ties proposed widgets to existing JSON types (e.g., sliders -> `range`, toggles -> `toggle/switch`, lists -> `feeder` pages).
* Identified where existing JSON widgets fall short (palette picker, true drop-down overlays, crosshair tile grids).

### 3) Conditional logic updates
* Replaced the `.menu`-style `condition` examples with JSON `show`/`enable` condition strings compatible with the current condition parser.

### 4) Typography configuration
* Clarified that the menu font is controlled by `globals.font` in `worr.json` (registered at UI init) rather than a `ui_font` cvar.

### 5) Expanded settings coverage
* Expanded video, audio, input, and HUD/view settings to better reflect existing renderer/client features (scale, HUD toggles, view bob, etc.).
* Added explicit Performance, Effects, and Screen sections aligned with current menu pages and their cvars.

### 6) Missing widget support enumerated
* Added a dedicated list of widget extensions required by the proposal (palette, drop-down overlays, crosshair tile selector, tabs, model preview, read-only status fields).

## Files updated
* `doc/proposals/menu_rework.md`
