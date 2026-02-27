# EAX Spatial Gap Closure (2026-02-27)

Task ID: `FR-06-T01`

## Summary
This update closes the remaining EAX spatial-awareness gaps identified after the
initial migration and follow-up fixes.

The two unresolved areas were:
1. Shared occlusion APIs were still stubbed to no-op in `src/client/sound/main.cpp`.
2. EAX zone selection still depended on center-point LOS only, causing false
   negatives in some layouts.

## Changes

### 1. Restored occlusion pipeline in shared sound path
File: `src/client/sound/main.cpp`

Implemented non-stub versions of:
- `S_ComputeOcclusion`
- `S_GetOcclusion`
- `S_SmoothOcclusion`
- `S_MapOcclusion`

Details:
- Added multi-ray occlusion sampling (direct + diffraction-weighted side probes).
- Added material/transparency weighting from trace surface metadata:
  - surface material tags (`csurface_t.material`)
  - transparency flags (`CONTENTS_WINDOW`, `SURF_TRANS33`, `SURF_TRANS66`)
- Added cutoff-frequency targeting per material class and smoothing toward
  `ch->occlusion_cutoff_target`.
- Added per-channel query rate limiting (`S_OCCLUSION_UPDATE_MS`).
- Added attack/release smoothing and strength/curve mapping.

Result:
- OpenAL and DMA paths now receive real occlusion factors/cutoff behavior again
  when `s_occlusion` is enabled.

### 2. Improved EAX zone reachability checks
File: `src/client/sound/al.cpp`

Added `AL_IsEAXZoneReachable(...)` and integrated it into
`AL_DetermineEAXEnvironment`.

Details:
- Keeps center-point LOS check as fast path.
- Adds fallback probe ring around zone origin (axis probes) when center LOS is
  blocked.
- Reduces false zone rejection where center is blocked but nearby zone volume is
  still acoustically reachable.

## Validation
- Build command:
  - `meson compile -C builddir-client-cpp20`
- Result:
  - Success (`worr.exe` rebuilt and linked).

## Files Changed
- `src/client/sound/main.cpp`
- `src/client/sound/al.cpp`
- `docs-dev/audio-eax-spatial-gap-closure-2026-02-27.md`
- `docs-dev/proposals/swot-feature-development-roadmap-2026-02-27.md`
