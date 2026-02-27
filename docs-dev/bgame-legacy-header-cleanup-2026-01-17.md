# Bgame Legacy Header Cleanup (2026-01-17)

## Summary
- Replaced legacy bgame headers with the WORR-kex C++ headers and normalized the module layout.
- bgame now lives under `src/game/bgame` and is shared by both sgame and cgame.

## File Moves and Removals
- Renamed `src/game/shared/` to `src/game/bgame/` to match the module name.
- Moved `m_flash.hpp` from `src/game/sgame/monsters/` into `src/game/bgame/` so muzzle flash data is owned by bgame.
- Removed legacy `.h` duplicates in `src/game/sgame/bots/` (`bot_debug.h`, `bot_exports.h`, `bot_includes.h`, `bot_think.h`, `bot_utils.h`).

## Code and Build Wiring
- Updated game module includes to reference `../bgame/` paths, and switched `src/game/cgame/cg_main.cpp` to include `m_flash.hpp`.
- Meson build wiring now uses `bgame_src` and adds `src/game/bgame` to both sgame and cgame include paths.

## Notes
- Engine-facing headers in `inc/shared/` (for example `m_flash.h`) remain unchanged to preserve engine API expectations.
- `src/legacy/game` is unchanged and still not built.

## Build Verification
- `meson compile -C builddir`
