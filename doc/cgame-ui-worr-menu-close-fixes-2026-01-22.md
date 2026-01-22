# Cgame UI WORR menu close fixes (2026-01-22)

## Summary
- Updated cgame JSON menus so close actions both dismiss the menu and clear sgame UI state.
- Ensured the UI list and match stats menus notify the server when leaving via the Return option.

## Problem details
- `closeCommand` is executed on Esc/right-click but does not automatically pop the menu.
- Several WORR menus defined `closeCommand` values that only informed the server (`worr_*_close`) and never closed the menu locally, leaving the menu stuck on screen.
- The `ui_list` and `match_stats` menus had a Return action that only executed `popmenu`, leaving sgame state (`ui.list.kind` / `ui.matchStatsActive`) set and causing blocking UI state checks to remain true.

## Fixes applied
- Added `popmenu` to close commands so Esc/right-click both close the menu and notify the server:
  - `ui_list`: `popmenu; worr_ui_list_close`
  - `vote_menu`: `popmenu; worr_vote_close`
  - `map_selector`: `popmenu; worr_mapselector_close`
  - `match_stats`: `popmenu; worr_matchstats_close`
- Updated Return actions to mirror close behavior:
  - `ui_list` Return now runs `popmenu; worr_ui_list_close`
  - `match_stats` Return now runs `popmenu; worr_matchstats_close`

## Impact
- Menus now reliably close with Esc/right-click.
- Server-side UI state is cleared when leaving list/stat menus, preventing UI blocking logic from persisting after the menu closes.

## Files touched
- `src/game/cgame/ui/worr.json`

## Verification notes
- Exercise `ui_list` (callvote map list, mymap list, setup gametype list) and confirm Esc/Return closes and restores input.
- Open vote, map selector, and match stats menus to confirm Esc closes the menu and the game is responsive afterward.
