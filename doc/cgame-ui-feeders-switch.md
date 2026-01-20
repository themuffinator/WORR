# Cgame UI Feeders and Switch Widget

## Overview
- Added JSON-driven feeders for built-in menu pages so server and demo browsers are registered from `worr.json` instead of hardcoded registration.
- Added a slide switch widget for ON/OFF toggles with keyboard/mouse interaction and bitfield support.
- Fixed demo browser list entries by using the correct extrasize so metadata renders.

## Feeder system
- `src/game/cgame/ui/ui_json.cpp` now accepts a `feeder` field on menu objects.
- Supported feeder ids: `servers`, `demos` (and `players` for parity with existing menu pages).
- When `feeder` is present, JSON `items` are ignored and a built-in page is registered via `CreateServerBrowserPage()` or `CreateDemoBrowserPage()`.
- `src/game/cgame/ui/worr.json` now defines `servers` and `demos` menus with `feeder` so `pushmenu servers`/`pushmenu demos` are script-driven.

## Switch widget
- New item type `switch` renders a modern slide switch with a track and knob.
- `src/game/cgame/ui/ui_widgets.cpp` implements `SwitchWidget` with:
  - left/right/mouse wheel to force off/on
  - enter/space/mouse click to toggle
  - `bit` and `negate` handling for bitfield cvars
  - state synced on open, applied on close (matching existing menu behavior).
- `src/game/cgame/ui/ui_json.cpp` maps `"type": "switch"` to the widget.
- `src/game/cgame/ui/worr.json` updates ON/OFF toggles to use `switch`.

## Demo list fix
- `src/game/cgame/ui/ui_page_demos.cpp` now sets `list_.extrasize = DEMO_EXTRASIZE` so demo metadata strings are read at the correct offsets.
