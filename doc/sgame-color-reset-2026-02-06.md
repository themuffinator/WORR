# Sgame Color Escape Resets (2026-02-06)

## Summary
Player-facing and console printouts that include player names now append a
white color reset (`^7`) after each name. This prevents color escapes in names
from tinting the remainder of the message.

## Key Changes
- Added the `G_ColorResetAfter` helper to centralize the “append ^7 unless it
  already exists” behavior for dynamic strings used in printouts.
- Wrapped player names with `G_ColorResetAfter` in sgame outputs, including
  localized broadcasts, center-print messages, and verbose console logging.

## Updated Print Sites (Not Exhaustive)
- Team change announcements (`BroadcastTeamChange`) now format with a safe name
  before center printing to other clients.
- Actor taunt chat lines (`m_actor.cpp`) append a reset after the player name.
- Admin add/remove announcements (`command_admin.cpp`) use a safe name when
  broadcasting the resolved player.
- Ghost-spawn failure diagnostics (`g_spawn_points.cpp`) wrap both the player
  name and blocking player name for console output.
- Chat macro overflow warnings and duel/gauntlet queue debug prints now use
  safe names in `Com_PrintFmt` output.
- Client config warnings that include player names now emit safe names so
  follow-up diagnostic text is not colored.

## Notes
- The reset is only applied to dynamic strings that can contain color escape
  sequences (player names). Static strings and map/entity identifiers are
  unchanged.
- Log-only events and config strings remain unmodified to preserve their raw
  values where color codes are not interpreted.
