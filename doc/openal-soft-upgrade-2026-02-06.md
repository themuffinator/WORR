# OpenAL Soft Upgrade (2026-02-06)

## Goal
Upgrade the Meson subproject dependency for OpenAL Soft to the latest available WrapDB revision.

## Previous State
- Wrap file: `subprojects/openal-soft.wrap`
- Previous pinned dependency: `openal-soft-1.23.1`
- Previous WrapDB revision: `1.23.1-2`

## Version Check
- `meson wrap info openal-soft` reported latest available version: `1.24.3-2`.

## Changes Applied
Updated `subprojects/openal-soft.wrap`:
- `directory`: `openal-soft-1.23.1` -> `openal-soft-1.24.3`
- `source_url`: `.../1.23.1.tar.gz` -> `.../1.24.3.tar.gz`
- `source_filename`: `openal-soft-1.23.1.tar.gz` -> `openal-soft-1.24.3.tar.gz`
- `source_hash`: updated to `7e1fecdeb45e7f78722b776c5cf30bd33934b961d7fd2a11e0494e064cc631ce`
- `source_fallback_url`: updated to the `openal-soft_1.24.3-2` WrapDB asset
- `patch_filename`: `openal-soft_1.23.1-2_patch.zip` -> `openal-soft_1.24.3-2_patch.zip`
- `patch_url`: updated to `v2/openal-soft_1.24.3-2/get_patch`
- `patch_hash`: updated to `6642f3f3321f3d00633547e3abc06657ca4e22124b6593da7bddd4d9593ccc2b`
- `wrapdb_version`: `1.23.1-2` -> `1.24.3-2`
- `[provide]`: `openal = openal_dep` replaced by `dependency_names = openal` (current WrapDB format)

## Validation
Commands executed:
1. `meson wrap update openal-soft`
2. `meson setup build-check --reconfigure`

Validation outcome:
- Meson reconfigure completed successfully.
- Meson pulled the updated source and patch during configure.
- Dependency resolution shows upgraded OpenAL:
  - `Dependency openal found: YES 1.24.3 (overridden)`

## Notes
- No engine/source code changes were required for this dependency update.
