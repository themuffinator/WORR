# Audio Doppler and Occlusion Fixes

## Summary
- Ensure occlusion smoothing uses a real frame delta by resetting `cls.frametime`
  after `S_Update`, not before it.
- Make doppler detection resilient to missing renderfx bits by falling back to
  projectile effect flags.
- Derive doppler velocities from `CL_GetEntitySoundOrigin` so motion matches the
  spatialized origin (avoids trail decimation artifacts at high FPS).

## Code Changes
- `src/client/main.cpp`: moved `cls.frametime = 0.0f;` to after `S_Update`.
- `src/client/sound/al.cpp`: added doppler detection helper and origin-based
  velocity sampling.

## Doppler Fallback Effects
When `RF_DOPPLER` is unavailable, doppler is enabled for entities with these
effects:
- `EF_ROCKET`
- `EF_BLASTER`
- `EF_BLUEHYPERBLASTER`
- `EF_HYPERBLASTER`
- `EF_PLASMA`
- `EF_IONRIPPER`
- `EF_BFG`
- `EF_TRACKER`
- `EF_TRACKERTRAIL`
- `EF_TRAP`
