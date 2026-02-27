# Dependency Rollup Upgrade (2026-02-06)

## Goal
Finish the remaining dependency updates after the SDL3/OpenAL/HarfBuzz/libjpeg-turbo/fmt upgrades, and validate with a clean Windows build.

## Dependencies Updated

### WrapDB-backed wraps
- `subprojects/cairo.wrap`
  - migrated to direct wrap file and pinned to `1.18.4-2`
- `subprojects/glib.wrap`
  - `2.84.4-1` -> `2.86.3-1`
- `subprojects/jsoncpp.wrap`
  - `1.9.5-2` -> `1.9.6-1`
- `subprojects/libpng.wrap`
  - `1.6.48-1` -> `1.6.53-2`
- `subprojects/pixman.wrap`
  - migrated to direct wrap file and pinned to `0.46.4-1`
- `subprojects/zlib-ng.wrap`
  - `2.2.4-1` -> `2.3.2-2`

### Non-WrapDB wraps
- `subprojects/libcurl.wrap`
  - `curl-8.15.0` -> `curl-8.18.0`
  - updated source hash to `40df79166e74aa20149365e11ee4c798a46ad57c34e4f68fd13100e2c9a91946`
- `subprojects/ffmpeg.wrap`
  - meson-port revision `meson-7.1` -> `meson-8.0.1`

## Build-System Compatibility Fix
- `meson.build`
  - zlib fallback options were updated for zlib-ng 2.3.x:
    - removed deprecated `force-sse2=true`
    - added `sse2=enabled`
  - reason: zlib-ng 2.3.2 no longer defines `force-sse2`, which caused subproject configuration failure and disabled zlib support in fallback builds.

## Clean Build Validation

### Clean configure/build commands
1. `meson setup build-clean-native-20260206c --native-file meson.native.ini`
2. `meson compile -C build-clean-native-20260206c`

### Validation results
- Build completed successfully (no link failures).
- Thin-archive linker failure was resolved by using the repo-native toolchain setup (`meson.native.ini` uses `tools/llvm-ar-no-thin.cmd`).
- Verified generated artifacts:
  - `build-clean-native-20260206c/worr.exe`
  - `build-clean-native-20260206c/worr.ded.exe`
  - `build-clean-native-20260206c/worr_opengl_x86_64.dll`
  - `build-clean-native-20260206c/worr_vulkan_x86_64.dll`
  - `build-clean-native-20260206c/cgamex86_64.dll`
  - `build-clean-native-20260206c/sgamex86_64.dll`
  - `build-clean-native-20260206c/subprojects/openal-soft-1.24.3/utils/openal-info.exe`
- Verified archives are non-thin (`!<arch>`) for:
  - `build-clean-native-20260206c/libq2proto.a`
  - `build-clean-native-20260206c/subprojects/openal-soft-1.24.3/libcommon.a`

## Notes
- `meson wrap status` may still report Cairo as `Have 1.18.4 1, but 1.18.4 2 is available` even though `subprojects/cairo.wrap` is pinned to `1.18.4-2`. This appears to come from local subproject metadata, not the committed wrap pin.
