# Client C++ Migration (WORR-KEX)

## Overview
- Migrated the client module to compile as C++ (C++latest), keeping behavior and APIs intact.
- Preserved the existing C-based core and q2proto (read-only) while keeping the client build minimal and focused.

## Scope
- Converted client and client sound sources from `.c` to `.cpp`.
- Left `src/client/null.c` as C (dedicated-server stub).
- No changes to `q2proto/` sources.

## Build System Updates
- `meson.build` client source lists updated to `.cpp` and conditional additions updated for curl/OpenAL/FFmpeg/gtv/software-sound.
- Added `client_cpp_args` with C++latest selection:
  - GCC/Clang: `-std=gnu++2b` fallback to `-std=c++2b`/`-std=gnu++23`/`-std=c++23`/`-std=gnu++20`/`-std=c++20`.
  - MSVC: `/std:c++latest`.
- Mirrored key engine defines/warnings into the client C++ build (matching existing C flags).

## Source Renames
Client module (`src/client/`):
- `ascii.c` -> `ascii.cpp`
- `cgame.c` -> `cgame.cpp`
- `cgame_classic.c` -> `cgame_classic.cpp`
- `console.c` -> `console.cpp`
- `demo.c` -> `demo.cpp`
- `download.c` -> `download.cpp`
- `effects.c` -> `effects.cpp`
- `entities.c` -> `entities.cpp`
- `input.c` -> `input.cpp`
- `keys.c` -> `keys.cpp`
- `locs.c` -> `locs.cpp`
- `main.c` -> `main.cpp`
- `newfx.c` -> `newfx.cpp`
- `parse.c` -> `parse.cpp`
- `precache.c` -> `precache.cpp`
- `predict.c` -> `predict.cpp`
- `renderer.c` -> `renderer.cpp`
- `screen.c` -> `screen.cpp`
- `tent.c` -> `tent.cpp`
- `ui_bridge.c` -> `ui_bridge.cpp`
- `view.c` -> `view.cpp`

Client sound module (`src/client/sound/`):
- `al.c` -> `al.cpp`
- `dma.c` -> `dma.cpp`
- `main.c` -> `main.cpp`
- `mem.c` -> `mem.cpp`
- `ogg.c` -> `ogg.cpp`
- `qal.c` -> `qal.cpp`

## Header C/C++ Interop
- Added `extern "C"` guards to client public headers to preserve C linkage:
  - `inc/client/client.h`, `inc/client/input.h`, `inc/client/keys.h`, `inc/client/ui.h`, `inc/client/video.h`
  - `inc/client/sound/dma.h`, `inc/client/sound/sound.h`
- Wrapped internal client headers used by C++ sources: `src/client/client.h`, `src/client/ui/ui.h`.
- Added `extern "C"` guards to common/system headers used directly by client C++ sources:
  - `inc/common/async.h`, `inc/common/bsp.h`, `inc/common/cmd.h`, `inc/common/cmodel.h`, `inc/common/common.h`
  - `inc/common/cvar.h`, `inc/common/error.h`, `inc/common/field.h`, `inc/common/fifo.h`, `inc/common/files.h`
  - `inc/common/intreadwrite.h`, `inc/common/math.h`, `inc/common/msg.h`, `inc/common/net/chan.h`, `inc/common/net/net.h`
  - `inc/common/pmove.h`, `inc/common/prompt.h`, `inc/common/protocol.h`, `inc/common/q2proto_shared.h`
  - `inc/common/sizebuf.h`, `inc/common/steam.h`, `inc/common/tests.h`, `inc/common/utils.h`, `inc/common/zone.h`
  - `inc/system/system.h`
- Extended `extern "C"` guards for additional common headers already used by UI and cgame: `inc/common/loc.h`, `inc/common/crc.h`, `inc/common/gamedll.h`, `inc/common/game3_convert.h`, `inc/common/game3_pmove.h`, `inc/common/hash_map.h`, `inc/common/mapdb.h`.

## C++ Compatibility Fixups
- Replaced C99 compound literals with C++-compatible initialization:
  - `inc/common/utils.h` (`Com_ComputeFrametime`) and `inc/common/intreadwrite.h` macros.
  - `src/client/cgame.cpp` (`cg_vec2_t`, `cgame_import_t` init).
- Added explicit casts for heap allocations returning `void *` when compiling as C++:
  - `src/client/screen.cpp`, `src/client/demo.cpp`, `src/client/download.cpp`, `src/client/locs.cpp`, `src/client/main.cpp`, `src/client/tent.cpp`.
  - `src/client/http.cpp`, `src/client/sound/al.cpp`, `src/client/sound/dma.cpp`, `src/client/sound/main.cpp`, `src/client/sound/ogg.cpp`.

## Notes
- `src/client/weapon_bar.cpp` and `src/client/wheel.cpp` remain excluded from the build per existing configuration.
- No behavioral changes intended; this is a mechanical migration to C++latest for the client module.

