# Cgame HUD Migration (Statusbar First)

## Goals
- Move HUD composition and draw logic into `src/game/cgame/cg_draw.cpp`.
- Keep legacy server/demo compatibility by retaining layout-string fallbacks.
- Avoid touching `q2proto/` and preserve classic servers/demos.

## Current Pipeline (Pre-migration)
- Statusbar: server builds layout in `src/game/sgame/gameplay/g_statusbar.cpp` and publishes it via `CS_STATUSBAR`.
- Scoreboard/EOU/help: server sends per-client `svc_layout` strings from `src/game/sgame/player/p_hud_scoreboard.cpp` and `src/game/sgame/player/p_hud_main.cpp`.
- Cgame renders layouts via `CG_ExecuteLayoutString` in `src/game/cgame/cg_draw.cpp`.

## Phase 1 (Implemented Here): Cgame-owned Statusbar
- Cgame builds the statusbar layout locally (mirrors `g_statusbar.cpp` logic) and draws it with `CG_ExecuteLayoutString`.
- Gated behind `cl_hud_cgame` to avoid behavior changes by default.
- Layout still uses `STAT_*` values and configstrings already populated by the server.

## HUD Blob (CS_GENERAL) Design
We reserve a contiguous CS_GENERAL range for a single HUD blob string:

- `CONFIG_HUD_BLOB` .. `CONFIG_HUD_BLOB_END` (12 segments, ~1152 bytes total).
- Server writes a single ASCII blob split across segments; cgame concatenates.
- Blob format is line-based and versioned:

```
HUD_BLOB v1
hud_flags 0x1
statusbar_flags 0x1
scoreboard_meta mode=ffa rows=16 team=0 limit=30 time=1234
scoreboard_row client=3 score=12 ping=40 flags=0
eou_meta rows=8 totals=1
eou_row map="unit1" kills=12/20 secrets=1/3 time=02:34:123
```

Flags (initial):
- `HUD_FLAG_MINHUD` (0x1): use min HUD layout (instagib/nadefest).

## Parsing Strategy
- Cgame caches each segment on configstring update.
- When any segment changes, rebuild blob and parse once.
- If the header/version is missing or parsing fails, fall back to legacy layouts.

## Compatibility & Fallback Rules
- Legacy servers/demos: blob absent => `CS_STATUSBAR` + `svc_layout` paths remain unchanged.
- New HUD paths are opt-in via `cl_hud_cgame` or blob flags when implemented server-side.
- `svc_layout` stays in use for scoreboard/EOU/help until cgame replacements are complete.

## Next Steps
- Emit HUD blob from sgame (scoreboard rows, EOU table, HUD flags).
- Replace `svc_layout` scoreboards with cgame rendering using blob data.
- Remove server-side statusbar layout string after parity validation.
