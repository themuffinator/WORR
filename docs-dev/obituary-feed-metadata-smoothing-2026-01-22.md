# Obituary Feed Metadata and Smoothing (2026-01-22)

## Goals
- Keep the kill feed capped at 4 entries, with smooth line movement and fade-out.
- Make obituary detection locale-safe by shipping structured metadata instead of parsing localized output.
- Expose lifetime and fade cvars for tuning.

## Metadata Format
Obituary prints embed metadata using ASCII control separators so the HUD can resolve icons and labels without text parsing.

- Base string decoration (sgame -> server):
  - RS (0x1e) + "OBIT" + US (0x1f) + <key> + US + <base>.
- Server broadcast (server -> client):
  - RS "OBIT" US <key> US <victim> US <killer> RS <visible_message>.

Keys use localization IDs where available (for example $g_mod_kill_blaster). Custom keys cover
unlocalized cases: obit_expiration, obit_self_plasmagun, obit_kill_plasmagun, obit_kill_plasmagun_splash,
obit_kill_thunderbolt, obit_kill_thunderbolt_discharge, obit_self_thunderbolt_discharge, obit_self_tesla.

## Server/Client Flow
- src/game/sgame/player/p_client.cpp: ClientObituary maps ModID to an obituary key and uses a
  decorated base string for LocBroadcast_Print. Existing log output continues to use the original
  English base strings.
- src/server/game.c: PF_Loc_Print strips the base decoration, localizes the base, then prepends the
  obituary metadata prefix before sending. PF_Broadcast_Print strips metadata before printing to the
  server console or MVD stream.
- src/client/parse.cpp: CL_HandlePrint strips metadata for console/notify HUD output but passes the
  original string to cgame->NotifyMessage so the kill feed can consume it.

## Cgame Handling
- src/game/cgame/cg_draw.cpp: CG_NotifyMessage parses obituary metadata first, then falls back to
  localized template parsing for legacy prints.
- New obituary templates add icon/label support for custom keys (w_plasmarifle, w_heatbeam, a_tesla;
  labels BLOOD, PLASMA, BOLT, TESLA).
- Entries store draw_y and animate toward target positions for smooth line transitions.

## Cvars
- cg_obituary_time (ms by default; values under 100 are treated as seconds)
- cg_obituary_fade (ms by default; values under 100 are treated as seconds)
- Setting cg_obituary_time to 0 disables the feed.

## Files
- src/game/sgame/player/p_client.cpp
- src/server/game.c
- src/client/parse.cpp
- src/game/cgame/cg_draw.cpp
- docs-dev/obituary-feed.md
