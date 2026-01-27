# Localization Key Renames (2026-01-27)

## Summary
- Renamed two non-human-friendly localization keys to descriptive names.
- Updated all localization files and code references to match the new keys.

## Renamed Keys
- `g_sgame_auto_501d9d3c3fe1` -> `match_map_vote_complete`
  - Text: ".Map vote complete!\nNext map: {} ({})\n"
- `e_auto_45ddf1902a53` -> `misc_cant_use_numeric_expressions`
  - Text: "Can't use '%s' with non-numeric expression(s)\\n"

## Files Updated
- `assets/localization/loc_*.txt` (all language files)
- `src/game/sgame/gameplay/g_map_manager.cpp`
- `src/common/cmd.c`
