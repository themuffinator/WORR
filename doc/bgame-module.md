# BGame Module Split

## Overview
- Introduced a dedicated `bgame` module for shared game code, modeled after the
  classic bg_ split in Quake-style engines.
- `bgame` sources are compiled into both `sgame` and `cgame`.

## Layout
- `src/game/bgame/`: shared headers and movement code (`bg_local.hpp`,
  `game.hpp`, `m_flash.hpp`, `p_move.cpp`, `q_std.hpp`, `q_vec3.hpp`).
- `src/game/sgame/`: server game implementation.
- `src/game/cgame/`: client game implementation.

## Build Wiring
- `meson.build` defines `bgame_src` and includes it in both `sgame_src` and
  `cgame_src`.
- `sgame_inc` and `cgame_inc` include `src/game/bgame` so shared headers resolve
  cleanly.
