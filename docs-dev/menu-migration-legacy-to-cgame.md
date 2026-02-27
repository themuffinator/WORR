# Menu Migration: Legacy Sgame to Cgame UI

## Summary
This change removes the old server-side menu system and routes match/welcome
menu behavior through the cgame JSON UI. The migration keeps the same entry
points while relying on cgame UI state flags and list/page widgets.

## Legacy Menu System Removal
- Deleted `src/game/sgame/menu/menu_main.cpp` and `src/game/sgame/menu/menu_system.cpp`.
- Removed `Menu`, `MenuEntry`, `MenuBuilder`, and related helpers from
  `src/game/sgame/g_local.hpp`.
- Simplified `gclient_t::menu` to a single scoreboard refresh timer and removed
  the unused `menu_sign` input field.

## UI State Helpers
- Added `IsUiMenuOpen(...)` to represent any cgame/initial menu that is visible.
- Added `IsBlockingUiMenuOpen(...)` for menus that should suppress server-side
  input (welcome/join/setup and tournament/setup list pages).
- `CloseActiveMenu(...)` now only clears cgame UI state and sends `forcemenuoff`.

## Input and Flow Updates
- `ClientThink` now gates view-angle freezing, freecam freeze, attack handling,
  and spectator follow input using `IsBlockingUiMenuOpen(...)`.
- `ClientBeginServerFrame` clears latched input only when blocking menus are open.
- Scoreboard refresh no longer depends on server-side menu rendering.
- Inventory and menu toggles (`Inven`, `PutAway`, `Score`, item selection) now
  use `IsUiMenuOpen(...)` to close or suppress menus cleanly.

## Build Cleanup
- Removed legacy menu sources from `meson.build` so only cgame UI menu pages
  remain in the sgame menu list.
