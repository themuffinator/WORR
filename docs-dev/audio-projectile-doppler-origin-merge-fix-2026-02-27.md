# Projectile Doppler Origin Merge Fix (2026-02-27)

Task reference: `FR-06-T01`

## Summary
- Fixed a loop-merge channel selection bug that could reuse Doppler-reserved projectile channels for merged non-Doppler sounds sharing the same sample.
- This could cause projectile loop sources to lose world-space tracking and behave like merged relative sounds, which made Doppler/projectile motion sound detached from projectile movement.

## Root Cause
- `AL_MergeLoopSounds` looks up an existing merge channel via:
  - `AL_FindLoopingSound(0, sfx)`
- `AL_FindLoopingSound` previously returned any autosound channel with that `sfx`, including channels marked `no_merge` (used to preserve per-entity Doppler behavior).
- When a merged group reused one of those channels, it overwrote source state for merged playback (`AL_SOURCE_RELATIVE`, merged pan/velocity) and cleared `no_merge`, breaking Doppler/world-origin behavior for that channel.

## Code Change
- File: `src/client/sound/al.cpp`
- Function: `AL_FindLoopingSound`
- Added a guard when selecting merge targets (`entnum == 0`):
  - skip channels with `ch->no_merge == true`.
- Result:
  - Merged loop sounds only reuse merge-eligible channels.
  - Doppler channels stay isolated and continue to spatialize from their projectile entity origins.

## Validation
- Full build completed successfully with Meson/Ninja:
  - `meson compile -C builddir-client-cpp20`
- No compilation errors introduced.

