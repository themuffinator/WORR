# fmt Upgrade (2026-02-06)

## Goal
Upgrade the Meson `fmt` dependency to the latest available WrapDB version.

## Previous State
- Wrap file: `subprojects/fmt.wrap`
- Previous source version: `fmt-11.0.2`
- Previous WrapDB revision: `11.0.2-1`

## Version Check
- `meson wrap info fmt` reported latest available release:
  - `12.0.0-1`

## Changes Applied
Updated `subprojects/fmt.wrap`:
- `directory`: `fmt-11.0.2` -> `fmt-12.0.0`
- `source_url`: updated to `https://github.com/fmtlib/fmt/archive/12.0.0.tar.gz`
- `source_filename`: `fmt-11.0.2.tar.gz` -> `fmt-12.0.0.tar.gz`
- `source_hash`: updated to `aa3e8fbb6a0066c03454434add1f1fc23299e85758ceec0d7d2d974431481e40`
- `source_fallback_url`: updated to WrapDB release asset for `fmt_12.0.0-1`
- `patch_filename`: `fmt_11.0.2-1_patch.zip` -> `fmt_12.0.0-1_patch.zip`
- `patch_url`: updated to `https://wrapdb.mesonbuild.com/v2/fmt_12.0.0-1/get_patch`
- `patch_hash`: updated to `307f288ebf3850abf2f0c50ac1fb07de97df9538d39146d802f3c0d6cada8998`
- `wrapdb_version`: `11.0.2-1` -> `12.0.0-1`
- `[provide]` entry changed from:
  - `fmt = fmt_dep`
  to:
  - `dependency_names = fmt`

## Validation
Commands executed:
1. `meson wrap update fmt`
2. `meson setup build-check --reconfigure`

Validation outcome:
- Meson configure completed successfully.
- Dependency resolution confirms updated `fmt`:
  - `Dependency fmt found: YES 12.0.0 (overridden)`
- OpenAL subproject also resolved against updated `fmt`:
  - `Dependency fmt found: YES 12.0.0 (overridden)` within `openal-soft`.

## Notes
- This is a major version bump (`11.x` -> `12.x`) at dependency level.
- No project source code changes were required during this update step.
