# Hit beeps + FPS font tweak

Date: 2026-01-28

## Summary
- FPS counter font now renders at ~5px line height for a minimal footprint.
- Hit marker beeps now support static or dynamic feedback with a new `cg_hit_beeps` cvar.
- Friendly-fire hits emit a distinct teammate beep.

## Details
- `src/game/cgame/cg_draw.cpp`
  - FPS counter now computes a per-draw scale factor to target a 5px line height.
  - Keeps existing HUD scaling behavior while shrinking the font.
- `src/client/main.cpp`
  - Added `cg_hit_beeps` (`1` = static, `2` = dynamic; default `1`).
  - Hit marker sound selection now uses `feedback/hit*.ogg` sounds with dynamic thresholds.
  - Friendly-fire hits use `feedback/hit_teammate.ogg`.
- `src/game/sgame/gameplay/g_combat.cpp`
  - Hit marker stat now encodes friendly-fire by storing a negative value when team damage occurs in a frame.
  - Total damage magnitude is preserved for dynamic beep selection.
- `src/client/precache.cpp`
  - Static hit beep now precaches `feedback/hit.ogg` in place of the legacy marker sound.

## Notes
- Sound paths use `feedback/*.ogg` (engine prepends `sound/`).
- If both friendly and enemy hits occur in the same frame, the friendly flag is retained so the teammate beep wins.
