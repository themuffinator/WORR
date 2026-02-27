# Engine + CGame Print Localization Expansion

This document records the localization work that expanded print-string coverage
across the client/engine and cgame UI layers. Only English keys were added for
now; other languages can follow once translations are available.

## Localization plumbing

- `src/common/common.c` now localizes `$`-prefixed print format strings in
  `Com_LPrintf` and `Com_Error` before formatting. This lets engine code use
  `$e_auto_*` keys while keeping existing printf-style arguments intact.
- `inc/client/cgame_ui.h` adds a `Localize` import for the cgame UI layer.
- `src/client/cgame.cpp` wires the UI `Localize` import to `CG_Localize`.
- `src/game/cgame/cg_ui_sys.cpp` localizes `$`-prefixed UI prints before
  formatting, so cgame UI messages can use `$cg_auto_*` keys with arguments.

## New localization keys

All new keys were added to `assets/localization/loc_english.txt` only.

- `cg_auto_*` (cgame/UI): menu parsing, UI usage/errors, and server browser
  feedback strings in `src/game/cgame` and `src/game/cgame/ui`.
- `e_auto_*` (engine/client): command system usage/errors, console commands,
  sound/music output, UI console helpers, connection/rcon/status output, client
  location commands, ascii screenshot flow, and renderer selection messaging.

## Coverage notes

- User-facing prints in the following areas now use localization keys:
  - Command subsystem (`src/common/cmd.c`)
  - Client connection/console flow (`src/client/main.cpp`, `src/client/console.cpp`)
  - Key binding help (`src/client/keys.cpp`)
  - Sound/music diagnostics (`src/client/sound/*.cpp`)
  - Client UI scripts and server lists (`src/client/ui/*.cpp`)
  - Location tools (`src/client/locs.cpp`)
  - ASCII screenshot flow (`src/client/ascii.cpp`)
  - Renderer selection (`src/client/renderer.cpp`)
  - CGame UI prints (`src/game/cgame/cg_ui_sys.cpp`, `src/game/cgame/ui/*.cpp`)
- Output-only table/list lines or raw string dumps remain as direct format
  strings to preserve user data and alignment, but surrounding labels now use
  localization keys.
