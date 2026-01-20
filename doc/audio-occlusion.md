# Audio Occlusion (Line-of-Sight Lowpass)

## Overview
The sound system supports optional occlusion: when a sound source is blocked by
world geometry, it is attenuated and low-pass filtered to give a muffled, behind-wall
effect. Occlusion is now computed as a partial factor (not just on/off) using
multiple rays and is smoothed over time to avoid flicker near edges.

## Cvar
- `s_occlusion` (default `1`): enables line-of-sight occlusion for sound effects.
- `s_occlusion_strength` (default `1.0`): scales occlusion intensity after smoothing.

## OpenAL backend
- Uses per-source `AL_FILTER_LOWPASS` filters to vary HF attenuation per channel.
- Applies per-source gain reduction via `AL_GAIN` and low-pass via
  `AL_DIRECT_FILTER`.
- Merged looping sounds (OpenAL merge mode) compute a weighted occlusion factor
  from contributing entities and smooth it across frames.
- Underwater filtering takes precedence for the direct filter when active.

Relevant code:
- `src/client/sound/al.cpp`
- `src/client/sound/main.cpp`

## DMA backend
- Uses a per-channel biquad low-pass filter in the mix path.
- Blends between dry and low-pass filtered samples based on occlusion factor.
- Merged looping sounds apply occlusion per entity contribution.

Relevant code:
- `src/client/sound/dma.cpp`
- `src/client/sound/main.cpp`

## Occlusion detection
- Multiple traces are cast from `listener_origin` to sample points around the
  sound origin and around the listener using `MASK_SOLID`.
- `SURF_SKY` hits are treated as unoccluded.
- `CONTENTS_WINDOW` and translucent surfaces reduce occlusion weight instead of
  fully blocking.
- Direct-path and peripheral samples are blended to approximate diffraction
  around corners.
- A dead-zone margin prevents very small occlusion fractions from applying.
- Full-volume sources (listener-local or `dist_mult == 0`) skip occlusion.

## Tunables
Occlusion strength is controlled by the constants in `src/client/sound/sound.h`:
- `S_OCCLUSION_GAIN` (volume attenuation)
- `S_OCCLUSION_LOWPASS_HZ` / `S_OCCLUSION_LOWPASS_Q` (DMA low-pass biquad)
- `S_OCCLUSION_AL_GAINHF` (OpenAL low-pass high-frequency gain)
- `S_OCCLUSION_UPDATE_MS` (occlusion trace rate limit)
- `S_OCCLUSION_CLEAR_MARGIN` (dead-zone threshold)
- `S_OCCLUSION_DIFFRACTION_WEIGHT` (blend between direct and peripheral rays)
- `S_OCCLUSION_CURVE` (response curve shaping small occlusion values)
- `S_OCCLUSION_ATTACK_RATE` / `S_OCCLUSION_RELEASE_RATE` (smoothing rates)
- `S_OCCLUSION_RADIUS_*` (sample radius)
