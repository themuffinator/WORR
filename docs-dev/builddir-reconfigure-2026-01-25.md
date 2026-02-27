# Builddir reconfigure and Windows clang archive fixes (2026-01-25)

## Summary
- Rebuilt the Meson builddir after clearing it to resolve clang/MSVC linker errors on static archives.
- Standardized jsoncpp (and fallback game config) to C++17 to satisfy MSVC header requirements.
- Fixed missing symbol visibility in shared input field logic and guarded UI HTTP fetch imports when libcurl is unavailable.

## Root cause
- The build used clang targeting MSVC with `llvm-ar`, which produced GNU-style `.a` archives. MSVC `link.exe` cannot consume those archives, resulting in LNK1107 errors.

## Resolution
- Reconfigured the build directory to use `llvm-lib` as the archiver and `llvm-ranlib` for indexing.
- Rebuilt the builddir to ensure static libraries are generated in a COFF-compatible format.

## Build steps
- Clear builddir and reconfigure with LLVM archiver tools:
  - `AR=llvm-lib`
  - `RANLIB=llvm-ranlib`
  - `meson setup --wipe builddir`
- Build with a single job if the link step reports file lock issues:
  - `meson compile -C builddir -j1`

## Code changes
- `meson.build`: default game fallback option now sets `cpp_std=c++17`.
- `subprojects/jsoncpp-1.9.5/meson.build`: default_options `cpp_std` updated to `c++17`.
- `src/common/field.c`: includes `common/zone.h` to declare `Z_Free`.
- `src/client/cgame.cpp`: wraps `.HTTP_FetchFile` assignment in `#if USE_CURL` and assigns `nullptr` otherwise.

## Notes
- The Meson configure still reports missing Windows resource compiler, which disables `zlib-ng` and `freetype2` subprojects. This limits protocol 35 and SDL3_ttf usage until a resource compiler is available.
