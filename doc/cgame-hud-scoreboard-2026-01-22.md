# Cgame HUD Scoreboard Blob (2026-01-22)

## Overview
- Adds a HUD blob scoreboard section emitted by sgame and parsed/drawn by cgame.
- Keeps legacy `svc_layout` scoreboard messaging intact for compatibility.

## HUD blob format (v1)
- `HUD_BLOB v1`
- `hud_flags 0x...`
- `sb_meta <mode> <flags> <score_limit> <red_score> <blue_score> <gametype>`
- `sb_row <client> <score> <ping> <team> <row_flags> <skin_icon>`

Notes:
- `gametype` is quoted when it contains spaces.
- The blob is clamped to `HUD_BLOB_MAX_SIZE` and truncated on full-line boundaries.

## Server emission
- `G_RunFrame_` updates `hud_flags` (minhud for instagib/nadefest).
- `MultiplayerScoreboard` builds `sb_meta`/`sb_row` lines and commits them via
  `G_HudBlob_SetScoreboardSection`.
- The blob is segmented into `CONFIG_HUD_BLOB` configstrings.

## Client parse and draw
- `CG_Hud_ParseBlob` reads `hud_flags`, `sb_meta`, and `sb_row`.
- `CG_DrawScoreboardFromBlob` builds a layout string using `client`/`ctf` tokens.
- Scoreboard drawing is gated by `cl_hud_cgame` and `LAYOUTS_LAYOUT`, with a
  fallback to the legacy layout string when blob data is missing.

## Compatibility
- Legacy servers and demos still use `CS_STATUSBAR` and `svc_layout`.
- The new configstring range is ignored by older clients.

## Follow-ups
- Add ready/flag icons and spectator sections in the cgame layout.
- Extend the blob with end-of-unit/intermission text if needed.
