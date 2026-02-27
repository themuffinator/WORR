# Audio Spatial Improvements (2026-01-19)

## Summary
- Added distance-based air absorption with EFX support and a low-pass fallback.
- Switched OpenAL distance attenuation to inverse model with tuned rolloff to preserve near-field loudness.
- Added material-specific high-frequency cutoffs to occlusion traces and smoothed cutoff targets per channel.
- Introduced per-source reverb send scaling (distance + occlusion) with auxiliary send filters.

## Air Absorption
- New cvars:
  - al_air_absorption (default 1)
  - al_air_absorption_distance (default 2048)
- When AL_AIR_ABSORPTION_FACTOR is supported, each source sets a factor based on distance.
- When unsupported, air absorption is folded into the direct low-pass filter (no extra effect slot required).
- Underwater mode disables air absorption to avoid double-filtering.

## Distance Model
- New cvar:
  - al_distance_model (0 = linear, 1 = inverse, default 1)
- Inverse distance uses rolloff `dist_mult * SOUND_FULLVOLUME` to match linear's near-field slope.
- Callback updates rolloff for active sources when the cvar changes.

## Material Transmission
- Occlusion traces now carry both weight and cutoff. Cutoff uses the most muffling hint with a default fallback.
- New/updated weights and cutoffs:
  - Glass/window: weight 0.25, cutoff 4000 Hz
  - Grate/mesh/fence/chain/grill/vent/screen: weight 0.30, cutoff clear (no HF loss)
  - Soft (cloth/curtain/fabric/carpet/plaster/drywall/sheetrock): weight 0.60, cutoff 2000 Hz
  - Wood/plywood: weight 0.75, cutoff 2000 Hz
  - Concrete/cement: weight 0.15, cutoff 800 Hz
  - Metal/steel/iron: weight 0.85, cutoff 1500 Hz
- OpenAL converts cutoff to AL_LOWPASS_GAINHF per source; DMA scales the base cutoff by the material ratio and rebuilds the per-channel biquad.

## Reverb Sends
- New cvars:
  - al_reverb_send (default 1)
  - al_reverb_send_distance (default 2048)
  - al_reverb_send_min (default 0.2)
  - al_reverb_send_occlusion_boost (default 1.5)
- Reverb send gain scales with distance and receives a boost for heavily occluded sounds.
- Auxiliary send filters are allocated per source to control send gain without affecting the direct path.
