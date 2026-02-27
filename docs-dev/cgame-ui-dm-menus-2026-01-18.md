# Deathmatch Cgame Menu Replacement (2026-01-18)

## Summary
- Replaced deathmatch welcome/join flow with JSON-driven cgame menus (mouse-enabled).
- Added dynamic label cvars, wrapped text widgets, and per-menu close commands in the cgame UI.
- Added server hooks to push cgame menus, populate UI cvars, and track cgame join state.

## New JSON menus
- `dm_welcome`: cgame welcome pop-up with hostname/MOTD, closeCommand triggers `worr_welcome_continue`.
- `dm_join`: deathmatch join menu using cvar-driven labels for team counts and join options.
- `dm_hostinfo`: read-only host information page (server name, host, MOTD).
- `dm_matchinfo`: read-only match settings page (gametype, map, ruleset, limits).

## Cgame UI extensions
- `labelCvar`: binds widget labels to a cvar value (escape-decoded) for dynamic text.
- `selectable`: optional JSON flag to make action/text items non-selectable.
- `wrappedtext`: new widget type for word-wrapped text blocks with `maxLines`.
- `closeCommand`: per-menu back/escape command hook in JSON.

## Server-side flow changes
- `OpenDmWelcomeMenu` and `OpenDmJoinMenu` now push cgame menus and set `ui_*` cvars.
- New `dmJoinActive` flag mirrors the existing `dmWelcomeActive` to avoid re-open spam.
- `worr_dm_join_close`, `worr_dm_hostinfo`, and `worr_dm_matchinfo` commands bridge cgame menus to server state.
- Deathmatch `OpenJoinMenu` dispatches to the cgame join menu; legacy join menus remain for non-DM.

## Notes
- Callvote/admin/stats legacy menus are not yet ported into cgame JSON.
