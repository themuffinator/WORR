# libjpeg-turbo Upgrade (2026-02-06)

## Goal
Upgrade the Meson `libjpeg-turbo` wrap dependency to the latest available WrapDB release.

## Previous State
- Wrap file: `subprojects/libjpeg-turbo.wrap`
- Previous pinned source: `libjpeg-turbo-3.1.0`
- Previous wrap revision: `3.1.0-1`

## Version Check
- `meson wrap info libjpeg-turbo` reported latest available version:
  - `3.1.3-1`

## Changes Applied
Updated `subprojects/libjpeg-turbo.wrap`:
- `directory`: `libjpeg-turbo-3.1.0` -> `libjpeg-turbo-3.1.3`
- `source_url`: updated from `.../3.1.0/...` to `.../3.1.3/...`
- `source_filename`: `libjpeg-turbo-3.1.0.tar.gz` -> `libjpeg-turbo-3.1.3.tar.gz`
- `source_hash`: updated to `075920b826834ac4ddf97661cc73491047855859affd671d52079c6867c1c6c0`
- `source_fallback_url`: now points to `v2/libjpeg-turbo_3.1.3-1/get_source/...`
- `patch_filename`: `libjpeg-turbo_3.1.0-1_patch.zip` -> `libjpeg-turbo_3.1.3-1_patch.zip`
- `patch_url`: updated to `v2/libjpeg-turbo_3.1.3-1/get_patch`
- `patch_fallback_url`: added and set to WrapDB GitHub release asset for `3.1.3-1`
- `patch_hash`: updated to `4c552c743b6a16d3338cbee6ab1dc10333f19a72464e7b7c964f6e03d78f48df`
- `wrapdb_version`: `3.1.0-1` -> `3.1.3-1`

`[provide]` remained unchanged:
- `dependency_names = libjpeg, libturbojpeg`

## Validation
Commands executed:
1. `meson wrap update libjpeg-turbo`
2. `meson setup build-check --reconfigure`

Validation outcome:
- Meson reconfigure succeeded.
- No dependency resolution errors were introduced by the wrap update.

## Notes
- In the current `build-check` config, `libjpeg` remains disabled (`libjpeg: NO` feature), so the reconfigure check validates wrap integrity/config stability rather than active `libjpeg` linking.
