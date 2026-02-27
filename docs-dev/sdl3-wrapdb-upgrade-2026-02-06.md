# SDL3 WrapDB Upgrade (2026-02-06)

## Goal
Upgrade WORR's SDL3 dependency to the latest available package revision while keeping the current SDL upstream release line.

## Baseline
- Previous project pin: `subprojects/sdl3.wrap` at `wrapdb_version = 3.4.0-1`.
- Previous SDL upstream release in use: `SDL3-3.4.0`.

## Version Check
- Verified SDL upstream tags from `https://github.com/libsdl-org/SDL.git`:
  - Latest `release-*` tag is still `release-3.4.0`.
- Verified WrapDB package revisions via `meson wrap info sdl3`:
  - Latest available wrap revision is `3.4.0-2`.

## Changes Made
Updated `subprojects/sdl3.wrap` from `3.4.0-1` to `3.4.0-2`:
- `source_fallback_url` now points to `sdl3_3.4.0-2`.
- `patch_filename` now uses `sdl3_3.4.0-2_patch.zip`.
- `patch_url` now points to `v2/sdl3_3.4.0-2/get_patch`.
- `patch_fallback_url` now points to the `sdl3_3.4.0-2` GitHub release asset.
- `patch_hash` updated to `0ef4e75d1a2416c71406d81cfbe5733caabfb86d1a2a551e62cd66e78bf23472`.
- `wrapdb_version` updated to `3.4.0-2`.

No source-level engine code changes were required for this dependency refresh.

## Validation
Commands run:
1. `meson wrap update sdl3`
2. `meson subprojects update --reset sdl3`
3. `meson setup build-check --reconfigure`

Validation result:
- Meson reconfigure succeeded.
- SDL3 fallback dependency resolves correctly (`Dependency sdl3 found: YES 3.4.0 (overridden)`).
- No SDL3 wrap revision mismatch warning remains after the reset/update step.

## Notes
- This update advances Meson WrapDB packaging metadata for SDL3 without changing the SDL upstream version (still `3.4.0`).
