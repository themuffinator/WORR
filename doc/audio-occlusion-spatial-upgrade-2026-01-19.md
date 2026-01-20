# Audio Spatial Occlusion Improvements (2026-01-19)

## Summary
- Tightened occlusion filtering and gain to better match real-world muffling.
- Increased ray sampling density by adding diagonal offsets in the occlusion probe.
- Added material-aware occlusion weights to distinguish glass/grates/soft surfaces from solid walls.

## Parameter Changes
- `S_OCCLUSION_LOWPASS_HZ`: 1200.0 -> 600.0
  - Software mixer low-pass cutoff now targets the 300-800 Hz range suggested for wall muffling.
- `S_OCCLUSION_AL_GAINHF`: 0.2 -> 0.08
  - OpenAL low-pass HF gain reduces high-frequency leakage under full occlusion.
- `S_OCCLUSION_GAIN`: 0.65 -> 0.35
  - Full occlusion now applies a ~65% level drop (previously ~35%).

## Sampling Improvements
- `s_occlusion_offsets` now includes diagonals in addition to cardinal axes.
- The occlusion solver now traces 16 rays per update (8 offsets from the source, 8 from the listener), improving coverage around the occlusion cross-section.

## Material-Based Weighting
- Occlusion traces now inspect `csurface_t.material` and apply a weight cap based on material hints.
- This weight is combined with existing window/translucent checks via `min()` so the lowest (most transmissive) hint wins.

### Current Weight Hints
- Glass/window: `S_OCCLUSION_GLASS_WEIGHT` (0.25)
- Grates/mesh/fence/chain/grill/vent/screen: `S_OCCLUSION_GRATE_WEIGHT` (0.3)
- Soft surfaces (cloth/curtain/fabric/carpet/plaster/drywall/sheetrock): `S_OCCLUSION_SOFT_WEIGHT` (0.6)
- Wood: `S_OCCLUSION_WOOD_WEIGHT` (0.75)
- Metal/steel/iron: `S_OCCLUSION_METAL_WEIGHT` (0.85)

## Notes
- Materials without a recognized hint default to full occlusion weight (1.0).
- Transparent surfaces flagged as `CONTENTS_WINDOW` or `SURF_TRANS33/66` still cap the occlusion weight as before.
