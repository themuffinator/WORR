# BGame Module Split

## Overview
- Introduced a dedicated `bgame` module for shared game code, modeled after the
  classic bg_ split in Quake-style engines.
- `bgame` sources are compiled into both `sgame` and `cgame`.

## Layout
- `src/game/bgame/`: shared headers and movement code (`bg_local.h`, `game.h`,
  `m_flash.h`, `p_move.cpp`, `q_std.h`, `q_vec3.h`).
- `src/game/sgame/`: server game implementation.
- `src/game/cgame/`: client game implementation.

## Build Wiring
- `meson.build` defines `bgame_src` and includes it in both `sgame_src` and
  `cgame_src`.
- `sgame_inc` and `cgame_inc` include `src/game/bgame` so shared headers resolve
  cleanly.
