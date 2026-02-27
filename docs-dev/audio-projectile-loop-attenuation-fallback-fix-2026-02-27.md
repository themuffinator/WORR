# Projectile Loop Attenuation Fallback Fix (2026-02-27)

Task ID: `FR-06-T01`

## Summary
Projectile loop sounds could remain over-attenuated when entities did not carry
an explicit `loop_attenuation` value. This was most visible for Doppler-enabled
projectile loops (for example rocket/energy hum loops), which could fade too
aggressively with distance.

## Root Cause
- Loop attenuation for entity autosounds is converted through
  `Com_GetEntityLoopDistMult(...)`.
- Some projectile loop entities can arrive with `loop_attenuation == 0` (unset
  for loop attenuation semantics).
- The generic fallback path treated this as non-normal loop attenuation,
  producing distance behavior that was too aggressive for projectile loops.

## Implementation
### File: `src/client/sound/main.cpp`
- Added `S_IsProjectileLikeLoopEntity(const entity_state_t *ent)`:
  - uses `RF_DOPPLER` and effect-based projectile heuristics (`EF_ROCKET`,
    `EF_BLASTER`, `EF_HYPERBLASTER`, `EF_PLASMA`, `EF_BFG`, `EF_TRACKER`,
    `EF_TRAP`, etc.).
- Added shared API `S_GetEntityLoopDistMult(const entity_state_t *ent)`:
  - if `loop_attenuation == 0` and entity is projectile-like, uses `ATTN_NORM`
    fallback.
  - otherwise uses entity-provided attenuation unchanged.
  - returns final multiplier via `Com_GetEntityLoopDistMult(...)`.

### File: `src/client/sound/sound.h`
- Replaced macro-only loop attenuation conversion with a declared shared
  function:
  - `float S_GetEntityLoopDistMult(const entity_state_t *ent);`

## Impact
- Projectile loop attenuation is now less aggressive when attenuation is unset.
- Fix applies uniformly to both sound backends:
  - OpenAL (`src/client/sound/al.cpp` call sites)
  - DMA (`src/client/sound/dma.cpp` call sites)

## Validation
- Build verified:
  - `meson compile -C builddir-client-cpp20`
- Result: success (`worr.exe` rebuilt and linked).

## Files Changed
- `src/client/sound/main.cpp`
- `src/client/sound/sound.h`
- `docs-dev/audio-projectile-loop-attenuation-fallback-fix-2026-02-27.md`
- `docs-dev/proposals/swot-feature-development-roadmap-2026-02-27.md`
