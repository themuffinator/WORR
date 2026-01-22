# Cgame HUD Server Pre-Parse (2026-01-22)

## Goal
Reduce direct configstring lookups inside `cg_draw.cpp` by caching key HUD
metadata as it arrives from the server, aligning with Q3-style draw flow where
UI rendering consumes pre-parsed state.

## Cached HUD state
Captured on configstring updates via `CG_Hud_ParseConfigString`:
- `CS_NAME` -> cached map name for scoreboard headers.
- `CS_GAME_STYLE` -> cached `game_style_t` for HUD and scoreboard gating.
- `CONFIG_HEALTH_BAR_NAME` -> cached raw health bar label key.
- `CONFIG_STORY` -> cached story key for the story panel.

## Draw path changes
- Scoreboard headers read map name from the cached HUD state instead of calling
  `cgi.get_configString` inside `cg_draw.cpp`.
- Health bar and story layout tokens use cached strings rather than raw
  configstring lookups.
- `CG_GetGameStyle` now returns cached state, avoiding per-frame parsing.

## Compatibility
Legacy layout rendering still uses `cgi.get_configString` for layout-driven
tokens (e.g., `stat_string`, `loc_string`) because those are explicitly driven
by layout scripts and stats; the cached state targets the primary HUD paths.
