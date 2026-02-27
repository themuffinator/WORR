# Build: Thin Archive + RC Fallback Fixes (2026-01-26)

## Summary
Windows builds were failing during static library creation and linking when
`llvm-ar` emitted thin archives (LNK1107 on `libq2proto.a`/OpenAL). This update
adds a wrapper to strip thin-archive flags, wires it into Meson, and fixes
resource compilation for zlib-ng/freetype when only `llvm-rc` is available.

## Changes
- Added `tools/llvm-ar-no-thin.py` and `tools/llvm-ar-no-thin.cmd` to strip
  `-T/--thin` flags before invoking `llvm-ar`.
- Updated `meson.native.ini` to point `ar` at the wrapper with an absolute path.
- `subprojects/zlib-ng-2.2.4/meson.build`: resource compilation now falls back
  to `llvm-rc` when `rc/windres` are missing, and includes both source and build
  directories so generated `zlib.h` is available to the RC preprocessor.
- `subprojects/freetype-2.14.1/meson.build`: resource compilation now falls
  back to `llvm-rc` when `rc/windres` are missing; added `_CRT_SECURE_NO_WARNINGS`
  to suppress CRT deprecation warnings.
- `src/game/cgame/ui/ui_list.cpp`: removed unused `widthTotal` to eliminate a
  compiler warning.

## Build Notes
- A fresh `builddir` was created after switching the archiver wrapper.
  Earlier directories were preserved as `builddir.old`, `builddir.failed`,
  and `builddir.failed2` to retain previous state.
