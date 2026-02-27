# Obituary Bold Names (2026-01-25)

## Summary
- Draw killer and victim names in the obituary feed with a faux-bold pass.
- Uses the existing HUD font path so text layout remains unchanged.

## Implementation
- Added a small helper that double-draws a string with a 1px (or scaled) offset.
- Applied the helper to obituary killer/victim strings only; labels/icons remain unchanged.

## Files Touched
- `src/game/cgame/cg_draw.cpp`

## Compatibility Notes
- Client-only rendering change; server protocol and demo compatibility unaffected.

## Testing
- Not run.
