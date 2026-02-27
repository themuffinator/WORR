# OpenAL EAX Spatial Awareness Fixes (2026-02-27)

Task ID: `FR-06-T01`

## Summary
This update fixes spatial-awareness regressions in the OpenAL EAX path where
non-merged channels were not receiving the same spatial effect routing as
merged loop channels.

## Problems Found
1. Per-source spatial effect updates (direct filter + auxiliary send shaping)
   were only executed in `AL_MergeLoopSounds`.
2. Non-merged channels were force-assigned to the EAX auxiliary send in
   `AL_Spatialize`, bypassing `al_reverb`/send filter control logic.
3. EAX zone selection used a hardcoded `8192.0f` nearest-distance cap.
4. EAX effect application success checks could be polluted by stale OpenAL
   error state.

## Implementation
File: `src/client/sound/al.cpp`

### 1. Unified per-channel spatial effect update path
- Added `AL_UpdateSpatialEffects(channel_t *ch, const vec3_t origin, bool fullvolume)`.
- This now runs for all non-merged channels from `AL_Spatialize`.
- The path applies:
  - occlusion query/reset policy
  - air absorption
  - direct filter updates (`AL_DIRECT_FILTER`)
  - auxiliary send routing/filter updates

### 2. Removed forced aux-send assignment from generic spatialize/play setup
- Removed direct `AL_AUXILIARY_SEND_FILTER` assignment blocks from:
  - `AL_Spatialize`
  - `AL_PlayChannel`
- Routing is now consistently handled through `AL_UpdateReverbSend`.

### 3. Made EAX zone nearest-match uncapped and cheaper
- Updated `AL_DetermineEAXEnvironment` to:
  - use `FLT_MAX` initial nearest distance
  - compare squared distances (`dist_sq`) before LOS trace
  - skip LOS traces for zones already outside radius or worse than current best
- This removes the 8192-unit implicit cap and cuts redundant traces.

### 4. Stabilized EAX update error reporting
- In `AL_SetEAXEffectProperties`, clear stale OpenAL error state with
  `qalGetError()` before applying properties.
- Return status now reflects the current update call more reliably.

## Behavior Impact
- Non-merged sounds now receive the same spatial send/filter pipeline as merged
  loop sounds.
- `al_reverb` and EAX send routing are consistent across merged and non-merged
  channels.
- Large-radius EAX zones beyond 8192 units can now be selected correctly.

## Validation
- Build validated with:
  - `meson compile -C builddir-client-cpp20`
- Result: success (`worr.exe` relinked).

## Files Changed
- `src/client/sound/al.cpp`
- `docs-dev/audio-eax-spatial-awareness-fixes-2026-02-27.md`
- `docs-dev/proposals/swot-feature-development-roadmap-2026-02-27.md`
