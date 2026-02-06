# Dependency warning cleanup (2026-02-06)

## Goal
Silence third-party dependency warnings in clean Windows fallback builds after the dependency upgrade rollup, while keeping engine warning policy intact.

## Changes
### `meson.build`
- Added global Windows CRT deprecation defines for all C/C++ compilation units:
  - `_CRT_SECURE_NO_WARNINGS`
  - `_CRT_NONSTDC_NO_WARNINGS`
- Added Windows clang/gcc-style global warning suppressions for fallback subprojects:
  - C++: `-Wno-deprecated-literal-operator`
  - C++: `-Wno-invalid-offsetof`
  - C++: `-Wno-unused-function`
  - C: `-Wno-unused-function`

## Why this was needed
- HarfBuzz fallback emitted Windows CRT deprecation warnings (`strncpy` family).
- OpenAL Soft fallback emitted warnings from vendored code (`deprecated literal operator`, `invalid-offsetof`, and `unused function` in utility sources).
- Subproject-local option overrides were not reliably propagating these suppressions in this environment, so top-level global arguments were used to guarantee application to fallback builds.

## Validation
### Clean reconfigure + rebuild
1. `meson setup build-clean-native-20260206i --native-file meson.native.ini`
2. `meson compile -C build-clean-native-20260206i`

### Warning scan
- `rg -n "warning:|: warning" build-clean-native-20260206i/compile.log`
- `rg -n -i "warning" build-clean-native-20260206i/compile.log`
- `rg -n "deprecated|invalid-offsetof|unused-function|strncpy" build-clean-native-20260206i/compile.log`

All scans returned no matches for dependency warning patterns.

## Notes
- `meson wrap status` reports all upgraded wraps up to date except a local Cairo status mismatch (`Have 1.18.4 1, but 1.18.4 2 is available`) despite `subprojects/cairo.wrap` already pinned to `1.18.4-2`.
