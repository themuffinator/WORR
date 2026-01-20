# Audio Occlusion Refinement

## Intent
Reduce overly aggressive occlusion (e.g., when a source is only slightly out of
view) while keeping a believable behind-wall effect and avoiding rapid flicker
from tiny edge intersections.

## What Changed
- Occlusion is now computed as a partial factor (0.0–1.0) using multiple rays
  around the source instead of a single binary line trace.
- A dead-zone margin ignores small occlusion fractions to prevent near-edge
  “false positives.”
- Occlusion is smoothed over time with separate attack/release rates and
  occlusion traces are rate-limited per channel to reduce CPU spikes.
- OpenAL uses per-source low-pass filters so HF rolloff can vary per channel.
- DMA blends between dry and low-pass filtered samples based on occlusion
  factor instead of a hard on/off filter.

## Detection Model
1. Build a local basis from the listener-to-source direction.
2. Sample multiple points around the sound origin (center, right/left, up/down).
3. Trace each ray with `MASK_SOLID`.
4. Count weighted hits; window/translucent hits contribute partial occlusion.
5. Normalize to an occlusion factor and apply a dead-zone threshold.

## Temporal Behavior
- Each channel caches its most recent occlusion result and only recomputes
  every `S_OCCLUSION_UPDATE_MS`.
- The visible occlusion factor is smoothed with attack/release rates to avoid
  frame-to-frame flicker on thin geometry.

## Tunables
All tuning constants live in `src/client/sound/sound.h`:
- `S_OCCLUSION_GAIN`
- `S_OCCLUSION_LOWPASS_HZ` / `S_OCCLUSION_LOWPASS_Q`
- `S_OCCLUSION_AL_GAINHF`
- `S_OCCLUSION_UPDATE_MS`
- `S_OCCLUSION_CLEAR_MARGIN`
- `S_OCCLUSION_ATTACK_RATE` / `S_OCCLUSION_RELEASE_RATE`
- `S_OCCLUSION_RADIUS_BASE` / `S_OCCLUSION_RADIUS_SCALE` / `S_OCCLUSION_RADIUS_MAX`
- `S_OCCLUSION_WINDOW_WEIGHT`

## Relevant Code
- `src/client/sound/main.cpp` (occlusion query, smoothing, rate limit)
- `src/client/sound/al.cpp` (per-source filter setup, occlusion gain/filter)
- `src/client/sound/dma.cpp` (dry/wet blend, per-channel filter state)
