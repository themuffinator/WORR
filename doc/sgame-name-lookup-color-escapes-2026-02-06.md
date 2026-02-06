# Sgame Name Lookup Ignores Color Escapes (2026-02-06)

## Summary
Player name lookups used by server commands now ignore Quake-style color
escapes (e.g., `^1`, `^a`) so names match regardless of embedded color codes.

## Key Changes
- Added `G_StripColorEscapes` to strip color escape sequences from user-supplied
  strings and stored player names before comparisons.
- Updated `ClientEntFromString` to compare sanitized names so commands that
  accept player names (admin, follow, system utilities) match colored names.
- Updated spectator name sanitization to strip color escapes before normalizing
  case and control characters.

## Notes
- This only affects name comparisons. Actual stored net names and config
  strings remain untouched.
