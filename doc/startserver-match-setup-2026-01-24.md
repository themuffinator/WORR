# Deathmatch Start Server Match Setup Alignment (2026-01-24)

## Summary
- Start Server deathmatch flow now uses the same match setup configuration set as the previous match setup wizard.
- Match setup wizard menus and commands removed; start server is the single entry point.
- Server start applies match setup choices at init time, including tournament config load and map reload for latched changes.

## UI changes
- `startserver` deathmatch options now include format, gametype, modifier, max players, match length, match type, and best-of (best-of only for tournament).
- Coop start server keeps legacy timelimit/fraglimit/maxclients inputs; deathmatch path uses match setup cvars.
- Start server action seeds default match setup values if blank and sets `match_setup_active` when starting deathmatch.

## Server/game changes
- New match setup cvars for format/gametype/modifier/maxplayers are registered alongside existing match setup cvars; defaults: regular/ffa/standard/8.
- `MatchSetup_ApplyStartConfig` applies deathmatch match setup on server init; tournament format loads the tournament config, otherwise applies format/modifier/length/type/best-of.
- Map reload happens after applying match setup if a latched cvar changed and a map is already set.

## Removed components
- Match setup wizard menus (`setup_*`) and client commands (`worr_setup_*`) removed.
- Host flow no longer opens the match setup wizard; admin menu hints updated to point to start server settings.

## Compatibility notes
- No protocol changes; legacy server connections and demos remain supported.
- Match setup is applied only when `deathmatch` is on and `match_setup_active` is set, leaving coop and legacy flows untouched.
