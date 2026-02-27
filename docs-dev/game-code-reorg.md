# Game Code Reorg

## Overview
- Moved the rerelease game sources out of the former submodule into `src/game/`.
- Archived the legacy baseq2 C game sources under `src/legacy/game` (not built).
- Removed the rerelease-game submodule from the build and repo tree.

## New Layout
- `src/game/bgame/`: shared headers and movement code (`bg_local.hpp`,
  `game.hpp`, `m_flash.hpp`, `p_move.cpp`, `q_std.hpp`, `q_vec3.hpp`).
- `src/game/sgame/`: server game implementation, including `bots/`, `ctf/`,
  `rogue/`, and `xatrix/`.
- `src/game/cgame/`: client game implementation and UI bridge (`cg_*`, `cg_ui_*`).

## Build Wiring
- `meson.build` now builds `sgame` and `cgame` directly from the new paths.
- `fmt` and `jsoncpp` dependencies are resolved from top-level wraps.
- `copy_sgame_dll` and `copy_cgame_dll` continue to stage DLLs into `builddir/baseq2`.

## Source Adjustments
- Updated `m_flash.hpp` includes in rogue/xatrix sources to use the bgame header path.
- cgame UI implementation now lives under `src/game/cgame`, with engine UI sources still
  compiled into cgame.

## Removal Notes
- `subprojects/rerelease-game` and its submodule entry were removed; the build no longer
  references that subproject.
