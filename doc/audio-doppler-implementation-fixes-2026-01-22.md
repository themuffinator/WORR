# Doppler Implementation Fixes (2026-01-22)

## Scope
Implements the recommended actions from `doc/proposed/doppler-implementation-analysis.md` to stabilize doppler playback for looped projectile sounds.

## Changes
### Source state initialization
- `AL_PlayChannel` now explicitly resets `AL_SOURCE_RELATIVE` to `AL_FALSE` and restores spatialize state for reused sources before spatialization. This prevents recycled sources from inheriting the merged loop's relative mode.

### Doppler loop merging
- `AL_MergeLoopSounds` now splits only entities that actually have doppler flags.
- Non-doppler entities with the same loop sound continue to merge into a single source, avoiding volume stacking while keeping doppler sources separate.

### Teleport discontinuity guard
- `AL_GetEntityVelocity` now treats large origin jumps (> 1024 units per sample) as discontinuities and resets doppler state, preventing extreme pitch shifts after teleports or snapping.

## Behavioral notes
- Doppler-enabled projectiles spatialize in world space even after merged loops have played.
- Mixed doppler and non-doppler loop groups no longer multiply loudness; only doppler-tagged entities stay unmerged.
- Teleporting entities do not produce a velocity spike in doppler pitch.

## Testing notes
- Fire several rockets simultaneously: combined loudness should stay closer to a single rocket while each doppler pitch shift remains distinct.
- Play a merged ambient loop, then fire a rocket: the rocket should not collapse to the listener position.
- Teleport an entity with doppler (if possible): pitch should not spike.
