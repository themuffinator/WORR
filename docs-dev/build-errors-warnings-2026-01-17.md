# Build Errors and Warnings Cleanup (2026-01-17)

## Overview
- Build completes cleanly after resolving cgame/sgame/client warnings and linker issues.

## cgame module fixes
- Switched cgame standard library includes to `q_std.hpp` and added a local `COM_ParseEx` implementation in `src/game/cgame/cg_std.cpp` so HUD/layout parsing links correctly.
- Added `vec2_t` compatibility by aliasing `Vector2` and adding `operator[]` in `src/game/bgame/game.hpp` for weapon wheel and HUD sizing code.
- Updated cgame import/pm_config field usage and export API version in `src/game/cgame/cg_main.cpp`, and pulled `monster_flash_offset` from `src/game/bgame/m_flash.hpp`.
- Normalized `get_configString` and frame time field names in cgame sources and aligned `cg_local.h` macros with the shared API.

## sgame module fixes
- Marked unused monster helpers, constants, and tournament/menu utilities as `[[maybe_unused]]` to silence `-Wunused` without removing code paths.
- Fixed enum switch coverage in HUD/menu/player code (`Team`, `WarmupState`, `MatchState`) and clarified control flow with braces/parentheses in `src/game/sgame/player/p_client.cpp`.
- Corrected social ID emptiness checks and item id validation to avoid tautological comparisons.
- Renamed the scrag-local `fire_acid` helper to `scrag_fire_acid` to avoid symbol collisions with the global `fire_acid`.
- Removed the `KEX_Q2GAME_DYNAMIC` guard in `src/game/sgame/q_std.cpp` so `Q_str*` and `Q_strl*` helpers are available in the DLL.

## client/engine warnings
- Reordered `cgame_import_t` designated initializers in `src/client/cgame.cpp` to match struct declaration order.
- Hoisted locals to avoid goto/initialization warnings in `src/client/input.cpp` and `src/client/sound/mem.cpp`.
- Replaced C99 array designators in `src/client/tent.cpp` with ordered initializers.
- Cleaned sound module warnings by using `{}` for `json_parse_t`, hoisting `sfxcache_t*`, reordering `snd_openal` initializers in `src/client/sound/al.cpp`, and suppressing clang function-pointer cast warnings in `src/client/sound/qal.cpp`.
- Updated `src/client/ui/playerconfig.cpp` to use explicit casts for static menu string literals to avoid writable-string warnings.

## Build result
- `meson compile -C builddir` completes with no errors or warnings.
