# Cgame UI JSON Menu System

## Overview
- The menu/UI system now runs inside cgame (`src/game/cgame/ui/`) instead of `client/ui/`.
- Scripted `.menu` assets are replaced with JSON menus (`worr.json`).
- The menu stack, widgets, and list browser pages are implemented in C++ and support paging/scrolling.

## Core architecture
- `src/game/cgame/ui/ui_core.cpp`: UI entry points, menu stack, input routing, push/pop commands.
- `src/game/cgame/ui/ui_menu.cpp`: Menu layout, focus, background/plaque/banner placement, scrolling.
- `src/game/cgame/ui/ui_widgets.cpp`: Widget implementations (actions, sliders, spins, fields, binds, save/load).
- `src/game/cgame/ui/ui_list.cpp`: Table/list widget used by server/demo browser.
- `src/game/cgame/ui/ui_json.cpp`: JSON loader that builds menus and widgets from `worr.json`.
- `src/game/cgame/ui/ui_page_servers.cpp`: server browser (master parsing, favorites, connect, ping).
- `src/game/cgame/ui/ui_page_demos.cpp`: demo browser.
- `src/game/cgame/ui/ui_page_player.cpp`: player config/preview page.
- `src/game/cgame/ui/worr.json`: menu definitions, embedded at build time.

## JSON file format (worr.json)
Root object:
- `globals`: defaults for background/font/cursor/weapon/colors.
- `menus`: array of menu definitions.

`globals` fields:
- `background`: color string (SCR_ParseColor) or image path.
- `font`: font name to register.
- `cursor`: cursor image path.
- `weapon`: model path used by player preview.
- `colors`: object with `normal`, `active`, `selection`, `disabled`.

Menu object fields:
- `name`: required menu id (used by `pushmenu <name>`).
- `title`: optional top title.
- `background`: color or image.
- `banner`: image displayed at top.
- `plaque`: string image name or object `{ "image": "...", "logo": "..." }`.
- `style`: object `{ "compact": bool, "transparent": bool }`.
- `items`: array of menu item objects.

Item expansion:
- String entries in `items` are allowed. If a string starts with `$$`, it expands a command macro or cvar contents into tokens.
- Object form `{ "expand": "$$name" }` provides the same expansion.

Item types and fields:
- `action`: `label`, `command`, `status`, `align` ("left" or default center).
- `bitmap`: `image`, `imageSelected` (optional), `command`.
- `range`: `label`, `cvar`, `min`, `max`, `step` (optional).
- `values`: `label`, `cvar`, `items` (string list).
- `strings`: `label`, `cvar`, `items` (string list).
- `pairs`: `label`, `cvar`, `pairs` array of `{ "label": "...", "value": "..." }`.
- `toggle`: `label`, `cvar`, `bit` (optional), `negate` (optional).
- `bind`: `label`, `command`, `status`, `altStatus`.
- `field`: `label`, `cvar`, `width`, `center`, `numeric`, `integer`.
- `blank`: separator spacer.
- `imagevalues`: `label`, `cvar`, `path`, `filter`, `imageWidth`, `imageHeight`.
- `episode_selector`: `label`, `cvar` (choices from mapdb episodes).
- `unit_selector`: `label`, `cvar` (choices from mapdb units).
- `savegame`: `slot`.
- `loadgame`: `slot`.

## Built-in pages
These are registered in C++ and are not defined in JSON:
- `servers` (server browser).
- `demos` (demo browser).
- `players` (player config/preview).

## Paging and input
- Menus auto-center, compute item layout each frame, and clamp focus to visible entries.
- Mouse wheel or arrow keys move focus; Page Up/Down scrolls a page.
- Large menus scroll vertically with `scrollY_`, and the focused item is kept in view.

## Embedding and defaults
- Default UI JSON filename: `APPLICATION ".json"` (currently `worr.json`).
- `meson.build` embeds `src/game/cgame/ui/worr.json` into the engine.
- `src/common/files.c` exposes the embedded `worr.json` to the filesystem layer.

## Migration notes
- `worr.menu` was replaced by `worr.json`; update any remaining references to use the JSON file.
- JSON menus live under `src/game/cgame/ui/` and are loaded at UI init.
