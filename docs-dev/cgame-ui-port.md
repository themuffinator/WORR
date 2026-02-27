# CGame + SGame Split

## Overview
- The rerelease game module now builds as two DLLs: server game (sgame) and client game (cgame).
- Only client binaries load cgame, and it is loaded from the base game directory.
- The menu UI now lives in cgame; the engine forwards UI calls through a thin bridge.

## Build Outputs
- Meson builds `sgame` and `cgame` from `src/game/sgame` and `src/game/cgame` with shared code in `src/game/bgame`.
- Custom targets copy both DLLs into `builddir/baseq2/`.
- Windows naming: `sgamex86_64.dll` and `cgamex86_64.dll` in baseq2.

## UI Port
- UI sources under `src/client/ui/` are compiled into cgame.
- `src/game/cgame/cg_ui_sys.c` provides engine-facing globals and wrappers (cmd buffer, `fs_gamedir`,
  `net_from`, `r_config`, Com/Cvar/FS/R/S entry points) backed by a UI import table.
- `src/client/ui_bridge.c` forwards `UI_*` calls to the cgame UI export.

## Extension API
- Cgame exposes UI via `CG_GetExtension(CGAME_UI_EXPORT_EXT)`.
- The engine provides `CGAME_UI_IMPORT_EXT` and checks `CGAME_UI_API_VERSION`.

## Loader Behavior
- `SGameDll_Load` uses `sgame` only (no legacy `game` fallback).
- `CGameDll_Load` follows the client DLL search order and starts from the executable basedir.
- `worr.json` is embedded in the engine filesystem for UI JSON loading.
