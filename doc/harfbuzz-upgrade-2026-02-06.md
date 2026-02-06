# HarfBuzz Upgrade (2026-02-06)

## Goal
Upgrade HarfBuzz to the latest available WrapDB version and keep Meson wrap resolution valid.

## Previous State
- `subprojects/harfbuzz.wrap` was a redirect:
  - `filename = freetype-2.14.1/subprojects/harfbuzz.wrap`
- Effective HarfBuzz source from redirect chain: `harfbuzz-11.4.1`.
- `subprojects/cairo.wrap` and `subprojects/glib.wrap` were also redirects into `harfbuzz-11.4.1/subprojects/...`.

## Version Check
- `meson wrap info harfbuzz` showed latest available version: `12.3.2-1`.

## Changes Applied
### 1) HarfBuzz wrap upgraded to direct WrapDB-managed file
`subprojects/harfbuzz.wrap` was converted from redirect to:
- `directory = harfbuzz-12.3.2`
- `source_url = https://github.com/harfbuzz/harfbuzz/releases/download/12.3.2/harfbuzz-12.3.2.tar.xz`
- `source_hash = 6f6db164359a2da5a84ef826615b448b33e6306067ad829d85d5b0bf936f1bb8`
- `source_fallback_url = https://wrapdb.mesonbuild.com/v2/harfbuzz_12.3.2-1/get_source/harfbuzz-12.3.2.tar.xz`
- `wrapdb_version = 12.3.2-1`

### 2) Cairo and GLib wraps converted to direct wrap files
Reason:
- After upgrading HarfBuzz, redirecting `cairo.wrap`/`glib.wrap` into a HarfBuzz subfolder was brittle.
- Meson validates redirect target paths early; when the target path was absent, configure failed before dependency download.

Resolution:
- `subprojects/cairo.wrap` updated to direct WrapDB file:
  - `directory = cairo-1.18.4`
  - `wrapdb_version = 1.18.4-2`
- `subprojects/glib.wrap` updated to direct WrapDB file:
  - `directory = glib-2.86.3`
  - `wrapdb_version = 2.86.3-1`

This removes coupling to a specific HarfBuzz unpacked folder path and makes wrap resolution deterministic.

## Validation
Command run:
1. `meson setup build-check --reconfigure`

Validation outcome:
- Configure succeeded.
- HarfBuzz resolved to upgraded version:
  - `Dependency harfbuzz found: YES 12.3.2 (overridden)`
- `SDL3_ttf` dependency also resolved against upgraded HarfBuzz:
  - `Dependency harfbuzz found: YES 12.3.2 (overridden)` in the `sdl3_ttf` subproject stage.

## Notes
- No engine/source code changes were required.
- This upgrade intentionally modernizes wrap handling away from redirect chains for `harfbuzz`, `cairo`, and `glib`.
