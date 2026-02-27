# WORR-kex Import and Wiring Notes

## Scope
- Integrate WORR-kex shared and server game code into the WORR tree.
- Align engine-facing ABI pieces (pmove, stats, powerup layout) with the KEX codebase.
- Preserve protocol compatibility by keeping q2proto unchanged.

## Source Import
- `src/game/bgame/` now mirrors WORR-kex shared headers and utilities.
- `src/game/sgame/` now mirrors WORR-kex server game code, replacing the previous module layout.

## Build System Touchpoints
- Meson sources updated to compile WORR-kex shared/sgame/cgame sources.
- Game DLLs are now built with the WORR-kex source tree and C++23 (sgame).

## Shared ABI / Stats Alignment
- `pmove_state_t` gains `haste` and `pm_flags` gains `PMF_HASTE` (bit 11).
  - Server sets `PMF_HASTE` when the Haste timer is active.
  - Client prediction derives `pm.s.haste` from `PMF_HASTE` to keep movement speed in sync.
  - No protocol changes were required; the flag rides existing pm_flags bits.
- `powerup_t` and `POWERUP_MAX` (29) are now defined in `inc/shared/shared.h` to match KEX.
  - `NUM_POWERUP_STATS` expands to cover the full powerup set; `STAT_POWERUP_INFO_END` shifts accordingly.
- The stats enum in `inc/shared/shared.h` now matches `src/game/bgame/bg_local.hpp`:
  - Adds KEX miniscore/teamplay stats and the post-32 extended stat range.
  - `MAX_STATS` is derived from `STAT_LAST`.
  - Legacy aliases are retained (`STAT_FRAGS`, `STAT_CHASE`, `STAT_TIMER_ICON`, `STAT_TIMER`).

## Cgame Wiring Adjustments
- Cgame now includes `bg_local.hpp` and uses `GetAmmoStat` / `GetPowerupStat` from shared code.
- Wheel/powerup accessors remain intact but now reflect the KEX stat layout.

## Notes
- q2proto is unchanged and remains read-only.
- Stat index changes are synchronized between engine (`inc/shared/shared.h`) and KEX headers (`src/game/bgame/bg_local.hpp`).
