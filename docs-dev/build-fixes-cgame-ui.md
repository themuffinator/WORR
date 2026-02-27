# CGame UI Build Fixes

## Overview
- Restored C++ compilation for the new cgame UI by fixing header dependencies, build-only helpers, and API glue.
- Made `Q_concat` usable from C++ translation units without changing existing call sites.
- Eliminated a handful of compile-time warnings introduced by the UI and renderer changes.

## Shared Helpers
- `inc/shared/shared.h` now provides a C++-safe `Q_concat` macro using a lambda that forwards to `Q_concat_array`. This avoids the C99 compound-literal usage that breaks C++ builds while keeping call sites unchanged.

## CGame UI Glue
- `src/game/cgame/ui/ui_internal.h` includes `common/common.h` and `client/input.h`, and declares the UI system bridge functions (`UI_Sys_UpdateTimes`, `UI_Sys_UpdateNetFrom`, `UI_Sys_UpdateRefConfig`, `UI_Sys_UpdateGameDir`), plus `SV_GetSaveInfo` and `UI_SetClipboardData`.
- `src/game/cgame/cg_ui_sys.cpp` exposes `UI_SetClipboardData` for UI call sites (non-static), matching the existing import hook.

## UI Behavior Fixups
- `src/game/cgame/ui/ui_widgets.cpp` now uses `Cvar_SetValue(..., FROM_MENU)` in slider teardown, matching the engine API.
- `src/game/cgame/ui/ui_page_servers.cpp`:
  - Copies and strips quotes from args safely before storing them.
  - Uses `UI_SetClipboardData` to avoid name collisions with WinAPI.
  - Replaces function-pointer parse selection with a simple `parse_binary` branch.
  - Adds a `List()` accessor used by sort helpers.
- `src/game/cgame/ui/ui_page_demos.cpp` exposes `UpdateSortFromCvar()` publicly so the cvar change callback can invoke it.

## Warning Cleanup
- `src/game/cgame/ui/ui_widgets.cpp` marks `previewWidth_` as intentionally unused to silence clang warnings.
- `src/rend_gl/main.c` removes the unused `dof_active` local in `GL_BindFramebuffer`.
