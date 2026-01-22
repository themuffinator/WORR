# SGAME command flood protection audit (2026-01-22)

## Context
The command dispatcher applied `CheckFlood` to every client command that did not
explicitly opt out. This caused non-chat commands (UI, inventory, admin, voting,
etc.) to be throttled and emit the "can't talk" flood message, which is not the
intended behavior.

## Decision
Flood protection remains limited to chat/gesture commands that actually emit
player-facing messages. Those commands already gate on `CheckFlood` internally
so they only count toward flood when a message is sent.

## Commands that use flood protection
- `say` (all chat)
- `say_team` / `steam` (team chat + alias)
- `wave` (gesture/point messages)

These commands keep the internal `CheckFlood` calls in
`src/game/sgame/commands/command_client.cpp` so flood is only counted when a
message is produced.

## Commands exempt from flood protection
All other sgame client commands are exempt, including:
- admin, voting, and cheat commands
- inventory and follow commands
- readiness and match setup commands
- WORR UI helpers and menu-driven commands

## Implementation notes
- `RegisterCommand` now defaults `floodExempt` to true so new commands are not
  throttled unless they explicitly opt in.
- `wave` is explicitly registered as flood-exempt since it performs its own
  flood gate.
