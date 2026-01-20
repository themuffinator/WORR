# Cgame Menu UI Upgrade (2026-01-18)

## Summary
- Added new widget types (dropdown, checkbox, combobox) and per-item conditions.
- Added menu page scrolling and list scrollbars for server info/players.
- Improved the player model preview with selectable weapon models.
- Reorganized menu layout to better cover gameplay and performance features.
- Replaced the deathmatch welcome flow with a cgame-driven pop-up.

## New widgets
- DropdownWidget uses SpinWidget data, draws a value with a down-arrow, and cycles with left/right/enter.
- SwitchWidget gains a checkbox style that renders [x]/[ ] while keeping toggle behavior for switches.
- ComboWidget combines a text field with a selectable list, cycles items with Tab, and writes to the target cvar.
- All widgets now honor IsDisabled() for draw and input so condition-based disables behave consistently.

## Conditions and flags
- Added MenuCondition parsing and evaluation in `ui_conditions.cpp` with AND semantics across conditions.
- `showIf` and `enableIf` accept a string or an array of strings in JSON.
- Condition syntax:
  - Built-ins: `ingame`, `in_game`, `in-game`, `deathmatch`, `dm`.
  - Cvar tests: `cvar:name`, `var:name`, or bare `name`.
  - Operators: `=`, `==`, `!=`, `>`, `>=`, `<`, `<=`, or existence check.
  - `!` prefix negates the condition.
- Added `ingameOnly` and `deathmatchOnly` item flags which map to show conditions.
- Menus refresh conditions on open and during layout to keep focus and scroll in sync.

## Menu scrolling
- Menu pages now draw a vertical scrollbar when content exceeds the visible area.
- The server browser info and players lists enable `MLF_SCROLLBAR` and adjust widths for the bar.

## Player model preview improvements
- Added a weapon dropdown to select weapon models from the player folder.
- Weapon list scans for `weapon.md2` and `w_*.md2`, builds labels, and syncs with the preview entity.
- Preview animation and muzzle flash now follow the chosen weapon model.

## Menu layout review
- Added a Performance menu with combobox FPS caps and checkboxes for prediction, footsteps, async, and fps rounding warnings.
- Converted many list and toggle items to dropdowns and checkboxes for consistent behavior across pages.
- Start server uses a combobox for map selection and a dropdown for rules.
- Game menu reorganized with conditional resume, save/load, disconnect, and deathmatch-only flags.

## Deathmatch welcome pop-up
- Added `dm_welcome` cgame menu page in `ui_page_welcome.cpp` with a compact transparent style.
- Server sends escaped hostname and MOTD to `ui_welcome_hostname` and `ui_welcome_motd` and issues `pushmenu dm_welcome`.
- New `worr_welcome_continue` command clears the initial freeze, then auto-joins or opens the join menu.
- `dmWelcomeActive` tracks the DM welcome state to prevent legacy menus from reopening.

## Build and registration
- Added `ui_conditions.cpp` and `ui_page_welcome.cpp` to `meson.build`.
- `MenuSystem::Init` registers `dm_welcome` if it is not present in the JSON.
