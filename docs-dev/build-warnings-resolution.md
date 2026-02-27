Build Warnings Resolution (Client + Linker)
===========================================

String handling cleanup:
- Replaced client-side `strcpy`/`strcat` usage with `Q_strlcpy`/`Q_strlcat`
  across core client, precache, and UI code paths to avoid deprecated CRT
  warnings while preserving truncation safety.
- Switched `demo.c` configstring reset to `Q_strlcpy` with the concrete
  configstring buffer size.
- Updated `Info_Print` in `src/shared/shared.c` to use `Q_strlcpy` for
  placeholder key/value strings.

CRT deprecation fixes:
- `http.c` now uses `Q_fopen` instead of `fopen` and reports IO errors via
  `Q_ErrorString(Q_ERRNO)` to avoid `fopen`/`strerror` deprecations.
- `precache.c` and `parse.c` use `sscanf_s` on Windows builds to silence
  `sscanf` deprecation warnings, with the existing `sscanf` retained for
  non-Windows builds.

Miscellaneous warning cleanup:
- Removed an unused `bits` variable from `CL_SendBatchedCmd` in
  `src/client/input.c`.
- Avoided `Cvar_Reset` macro redefinition warnings by undefining it before
  mapping renderer imports in `inc/renderer/renderer_api.h`.

Linker warning fixes (Windows + clang):
- Updated `meson.build` to detect MSVC-style linkers (`link`/`lld-link`)
  and pass `/NXCOMPAT`, `/DYNAMICBASE`, `/HIGHENTROPYVA`, and `/BASE:...`
  via `-Wl,` instead of GNU `--nxcompat` flags.
- Limited `-static-libgcc` to GNU-style linkers to prevent clang warnings
  about unused linker arguments.
