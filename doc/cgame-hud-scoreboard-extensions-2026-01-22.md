# Cgame HUD Scoreboard/EOU Extensions (2026-01-22)

## Overview
- Adds scoreboard header/footer metadata and spectator lists to the HUD blob.
- Adds end-of-unit (EOU) data to the HUD blob and renders the table in cgame.
- Keeps legacy `svc_layout` scoreboard/EOU messages as fallbacks.

## HUD blob additions
- Scoreboard lines:
  - `sb_host <hostname>`
  - `sb_time <formatted_time_line>`
  - `sb_victor <victor_message>`
  - `sb_press <server_frame_gate>`
  - `sb_state <in_progress>`
  - `sb_queue <client> <wins> <losses>`
  - `sb_spec <client> <score> <ping>`
- EOU lines:
  - `eou_row <name> <kills> <total_kills> <secrets> <total_secrets> <time_ms>`
  - `eou_total <kills> <total_kills> <secrets> <total_secrets> <time_ms>`
  - `eou_press <server_frame_gate>`

## Server emission
- `MultiplayerScoreboard` now emits header/footer metadata and spectator lists.
- Team rows set `SB_ROW_FLAG_RED/BLUE` when carrying enemy flags.
- `EndOfUnitMessage` builds and publishes `eou_row`/`eou_total`/`eou_press`.
- `G_RunFrame_` clears the EOU section when not in an end-of-unit intermission.

## Cgame rendering
- Scoreboard layout now shows:
  - Hostname or intermission match-time line.
  - Victor message and gated press-button prompt.
  - Ready icons (FFA/Duel + team ready-up) and flag icons for carriers.
  - Spectator sections for duel/team modes (queued + passive).
- EOU layout is reconstructed from `eou_*` lines using `start_table/table_row`.
- `cl_hud_cgame` still gates the cgame-side scoreboard/EOU render path.

## Compatibility
- Legacy servers/demos continue to use `CS_STATUSBAR` and `svc_layout`.
- Blob extensions are ignored by older clients.
