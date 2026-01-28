# CGame Entity Port (2026-01-28)

## Summary
Client entity handling and related effects/temp entities are now wired through a cgame extension, enabling the cgame module to own entity parsing, interpolation, and rendering submission while retaining legacy server/demo compatibility. Engine-side code keeps a fallback path when the extension is unavailable.

## New CGame Extension
- **Extension name:** `CGameEntity_Export_v1` / `CGameEntity_Import_v1`
- **API version:** `1`
- **Headers:** `inc/client/cgame_entity_ext.h`, `inc/client/cgame_entity.h`
- **Import payload:** Pointers to `cl`, `cls`, `cl_entities`, temp entity params, renderer/sound hooks, prediction helpers, and utility functions.
- **Export payload:** Entry points for effects, temp entities, parsing, view calc, entity add, and debug checks.

## Engine Wiring
- **New shared client state header:** `inc/client/client_state.h` holds `centity_t`, `client_state_t`, `client_static_t`, `server_frame_t`, etc.
- **Engine cgame bridge:** `src/client/cgame.cpp`
  - Provides `cg_entity_import` via `CG_GetExtension`.
  - Caches `cgame_entity` export when available.
  - Calls `CL_InitEffects`, `CL_InitBrightskins`, `CL_InitTEnts` after successful cgame entity load to ensure cgame-side initialization.
- **Engine dispatch:** `src/client/entities.cpp`, `src/client/effects.cpp`, `src/client/tent.cpp`
  - Early dispatch to `cgame_entity` where available; otherwise keeps legacy engine behavior.

## CGame Entity Module
- **Local wiring header:** `src/game/cgame/cg_entity_local.h`
  - Maps engine imports to local macros (`Com_*`, `Cvar_*`, `R_*`, `S_*`, `CL_*`, etc).
  - Provides `cl`, `cls`, `cl_entities`, `te`, `mz`, `cmd_buffer`, and frame timing macros.
  - Overrides `EXEC_TRIGGER` to route through `AddCommandString`.
- **API glue:** `src/game/cgame/cg_entity_api.cpp`
  - Stores `cgei` import pointer and exposes `CG_GetEntityAPI`.
  - Initializes shared cvar pointers (`cl_*`, `info_*`) via `CG_Entity_InitCvars`.
- **Ported sources:**
  - `src/game/cgame/cg_entities.cpp` (from `src/client/entities.cpp`)
  - `src/game/cgame/cg_effects.cpp` (from `src/client/effects.cpp`)
  - `src/game/cgame/cg_newfx.cpp` (from `src/client/newfx.cpp`)
  - `src/game/cgame/cg_tent.cpp` (from `src/client/tent.cpp`)
- **Notable adjustments:**
  - `CL_AddParticles` now uses `V_AddParticle` instead of renderer globals.
  - Null-surface checks use imported `null_surface` instead of `nulltexinfo`.
  - Muzzle flash offsets use a helper (`CL_AddPlayerMuzzleFXv`) to avoid C99 compound literals.
  - `cl_timeout_changed` is implemented locally for railtrail timing.

## Build System
- **Meson updates:** `meson.build`
  - Added new cgame source and header files to `cgame_src`.

## Compatibility Notes
- Legacy Q2 servers and demos remain supported: engine falls back to existing entity handling when the cgame entity extension is absent.
- `q2proto/` remains untouched; cgame entity uses the imported `Q2Proto_UnpackSolid` wrapper to preserve protocol compatibility.
