# Download Overlay + Match Menu Blur Changes

## Summary
- Default startup now opens the main menu via `ui_open` (default set to `1`).
- Added a JSON-driven download overlay menu with a centered frame and a progress bar.
- Added client-side UI cvars that drive download status text and progress.
- Download overlay suppresses the legacy console download bar while active.
- Hint bar now skips Back/Select prompts on menus with no selectable items.
- Match-related menus now disable menu bokeh blur by default.

## UI JSON additions
- `style.blur`: opt in/out of menu bokeh blur per menu.
- `style.frame`: optional framed panel behind menu content.
  - `fill`: frame fill color.
  - `border`: frame border color.
  - `padding`: frame padding in UI pixels.
  - `borderWidth`: frame border thickness in UI pixels.
- `progress`: new widget type that renders a cvar-driven progress bar.

## Download overlay behavior
- New menu: `download_status` in `src/game/cgame/ui/worr.json`.
- Client updates these cvars while downloads are queued/active:
  - `ui_download_active`
  - `ui_download_file`
  - `ui_download_status`
  - `ui_download_queue`
  - `ui_download_percent`
- Client forces the download overlay while downloads are pending and the game is not active.

## Match menu blur
Match-related menus disable menu bokeh blur by default. The current match menu set includes:
- `dm_*`
- `callvote_*`
- `mymap_*`
- `forfeit_*`
- `admin_*`
- `setup_*`
- `tourney_*`
- `vote_*`
- `map_selector`
- `match_stats`
