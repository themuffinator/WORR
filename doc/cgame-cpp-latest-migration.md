# Cgame C++latest Migration

## Goal
- Move cgame/UI sources to C++ and compile the cgame DLL with C++latest while keeping changes minimal.

## Source Renames
- `src/game/cgame/cg_ui_export.c` -> `src/game/cgame/cg_ui_export.cpp`
- `src/game/cgame/cg_ui_sys.c` -> `src/game/cgame/cg_ui_sys.cpp`
- `src/client/ui/demos.c` -> `src/client/ui/demos.cpp`
- `src/client/ui/menu.c` -> `src/client/ui/menu.cpp`
- `src/client/ui/playerconfig.c` -> `src/client/ui/playerconfig.cpp`
- `src/client/ui/playermodels.c` -> `src/client/ui/playermodels.cpp`
- `src/client/ui/script.c` -> `src/client/ui/script.cpp`
- `src/client/ui/servers.c` -> `src/client/ui/servers.cpp`
- `src/client/ui/mapdb.c` -> `src/client/ui/mapdb.cpp`
- `src/client/ui/ui.c` -> `src/client/ui/ui.cpp`

## Build Changes
- `meson.build` updates `ui_src` and `cgame_src` to the new `.cpp` filenames.
- `meson.build` adds `cpp_std=c++latest` to the cgame DLL `override_options` so only cgame uses the latest standard.

## C++ Compatibility Fixes
- `src/game/cgame/cg_ui_export.cpp` switches to positional aggregate init and marks `CG_GetUIAPI` as `extern "C"`.
- `src/game/cgame/cg_ui_sys.cpp`:
  - Marks `CG_UI_SetImport` as `extern "C"` to keep the export ABI stable.
  - Replaces the C designated initializer for `vid_driver_t` with a C++ lambda initializer.
  - Avoids returning string literals as `char *` by using `cmd_null_string`.
- `src/client/ui/*.cpp`:
  - Adds explicit `static_cast<>` for `UI_Malloc`, `UI_Mallocz`, `Z_Realloc`, and `UI_FormatColumns` call sites.
  - Replaces the C compound literal in demos date formatting with a `time_t` local.
  - Uses `const_cast<char **>` for static string tables (`yes_no_names`, `handedness`).

## Notes
- Shared/common support sources still compile as C; the migration here is scoped to cgame/UI-specific code.
