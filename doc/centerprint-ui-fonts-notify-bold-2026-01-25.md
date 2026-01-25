# Center Print cl_font and Notification Bold Names (2026-01-25)

## Summary
- Isolate `cl_font` to center print rendering only.
- Switch general screen/HUD text to `fonts/AtkinsonHyperLegible-Regular.otf`.
- Bold chat speaker names in the notification area for `say`/`say_team`-style messages.

## Font Routing Changes
- Added a dedicated UI font handle for general screen rendering (Atkinson HyperLegible).
- Kept `cl_font` as the center print font; center print drawing now uses a separate path.
- Updated screen text drawing helpers and macros to use the UI font handle by default.
- Added center-font measurement/draw helpers to the cgame import API to keep center prints on `cl_font` while the rest of the HUD uses the UI font.

## Notification Area Bold Names
- Chat notification lines are split on the first colon (`:`) and the name portion is drawn with a faux-bold pass (double-draw with a 1px offset).
- This applies to notification lines flagged as chat in both the classic screen notify HUD and the cgame notify list.

## Files Touched
- `inc/shared/game.h`: added center-font draw/measure hooks.
- `src/game/bgame/game.hpp`: mirrored new cgame import hooks.
- `src/client/client.h`: added UI font handles and switched `SCR_DrawString` to UI font.
- `src/client/screen.cpp`: UI font loading, notify bold draw helper, UI font routing.
- `src/client/cgame.cpp`: UI font routing and new center-font helpers.
- `src/game/cgame/cg_draw.cpp`: center print uses center font helpers; notify name bold.
- `src/client/wheel.cpp`, `src/client/weapon_bar.cpp`: text rendering uses UI font handle.

## Compatibility Notes
- Legacy servers/demos remain compatible; text draw routing is client-side only.
- `cl_font` cvar behavior is preserved for center prints.
- UI font load falls back to existing font paths if the Atkinson file is unavailable.

## Testing
- Not run (font path availability depends on data pack setup).
