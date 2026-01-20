# Audio Occlusion and Doppler Quality Upgrade

## Goals
- Restore audible occlusion and doppler behavior under default settings.
- Move occlusion toward a higher-quality, diffraction-aware model.
- Make doppler stable under net jitter and avoid pitch spikes.

## Occlusion Changes
- Added a direct + peripheral sampling model: rays are cast around both the
  source and the listener, then blended to approximate diffraction.
- Reduced the dead-zone threshold so partial occlusion is no longer swallowed
  by the clear margin.
- Introduced a response curve to make small occlusion values more audible.
- Separated raw occlusion state from mix occlusion to keep smoothing stable in
  DMA while still shaping the audible response.

### Controls
- `s_occlusion_strength` scales the final occlusion intensity after smoothing.
- `S_OCCLUSION_DIFFRACTION_WEIGHT` blends direct and peripheral rays.
- `S_OCCLUSION_CURVE` shapes the response (values < 1.0 increase audibility).

## Doppler Changes
- Per-entity doppler now tracks smoothed, clamped velocity derived from
  interpolated positions to reduce jitter and teleport spikes.
- Added tunables for speed-of-sound, min speed, max speed, and smoothing rate.
- Loop merging now bypasses `RF_DOPPLER` sounds so per-entity velocities are
  preserved when doppler is desired.

### Controls
- `al_doppler_speed` sets speed of sound (units/sec).
- `al_doppler_min_speed` and `al_doppler_max_speed` gate and clamp velocity.
- `al_doppler_smooth` applies exponential smoothing to source/listener motion.

## Implementation Notes
- OpenAL applies the occlusion mapping for both gain and low-pass filtering.
- DMA keeps raw occlusion for smoothing and uses a separate `occlusion_mix`
  value for filtering and attenuation.
- `no_merge` marks looped channels that must bypass merge to keep doppler.

Relevant code:
- `src/client/sound/main.cpp`
- `src/client/sound/dma.cpp`
- `src/client/sound/al.cpp`
- `src/client/sound/sound.h`
