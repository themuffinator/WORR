# Client entity port build fixes (2026-01-28)

## Summary
This document captures build-fix work that unblocked the client-entity port from `client/` into `cgame/` while preserving legacy Q2 compatibility. The focus was to restore missing imports, prototypes, and ordering issues introduced by the split.

## Changes

### cgame import surface expanded
- Added memory and filesystem hooks to the cgame entity import:
  - `Z_Malloc`, `Z_Freep`
  - `FS_LoadFileEx`, `FS_FreeFile`
- Wired the import table in `src/client/cgame.cpp` to pass engine implementations.
- Added cgame-local macros in `src/game/cgame/cg_entity_local.h` for `Z_Malloc`, `Z_Freep`, `FS_LoadFile`, and `FS_FreeFile` so existing cgame code keeps its legacy call sites.

### cgame effect/prototype coverage
- Added missing forward declarations in `src/game/cgame/cg_client_defs.h` for client-style effects used by the ported files (temp entities, particle trails, trackers, etc.).
- Corrected prototypes to match actual definitions (e.g., constness and `unsigned int` color parameter) so the cgame DLL links cleanly.

### Renderer build ordering fix
- Added a forward declaration for `GL_DlightInfluenceRadius` in `src/rend_gl/gl.h` to avoid implicit declaration errors when used before its definition.

## Files touched
- `inc/client/cgame_entity.h`
- `src/game/cgame/cg_entity_local.h`
- `src/game/cgame/cg_client_defs.h`
- `src/client/cgame.cpp`
- `src/rend_gl/gl.h`

## Build result
- `ninja -C builddir` completes successfully after these changes.
