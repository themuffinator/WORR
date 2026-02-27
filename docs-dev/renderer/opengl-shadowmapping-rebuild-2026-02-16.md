# OpenGL Shadowmapping Rebuild (2026-02-16)

## Intent
Rebuild `rend_gl` shadowmapping from a clean baseline guided by:
- `docs-dev/proposals/shadowmapping_implementation_guide.md`
- `docs-dev/proposals/shadow_mapping_analysis.md`

The goal is to replace the recent complex slot-churn/fallback stack with a
deterministic, correctness-first implementation.

## Non-negotiable constraints
1. Keep shadow rendering independent from camera-visible entity lists.
2. Keep a dedicated shadow-caster list each frame.
3. Preserve safe-origin handling for lights in solid.
4. Keep cache behavior explicit and simple:
   - `gl_shadowmap_cache_mode 0`: no cache reuse.
   - `gl_shadowmap_cache_mode 1`: static cache invalidates when dynamic casters
     intersect.
   - `gl_shadowmap_cache_mode 2`: world-only static cache reuse.
5. Keep per-light shadow assignment deterministic and stable in a single frame.
6. Avoid renderer behavior that depends on no-slot unshadowed fallback hacks.

## Rebuild scope
### In scope now
- `src/rend_gl/main.c`: shadow light selection, cache invalidation, render pass
  orchestration, debug counters.
- `src/rend_gl/gl.h`: shadow state cleanup for removed complexity.
- `src/rend_gl/texture.c`: shadow cache metadata initialization/shutdown cleanup.
- `src/rend_gl/shader.c`, `src/rend_gl/sp3b.c`: shadow upload contract cleanup.

### Explicitly out of scope for this pass
- Replacing existing shader filtering models (PCF/VSM/EVSM/PCSS).
- Replacing existing CSM math/projection path.
- Full BSP shadow-surface marking rewrite.

## Old behavior being removed
1. No-slot static-light fallback path (`gl_shadowlight_no_slot_mode`).
2. Sticky/hysteresis/PVS-priority slot churn tuning cvars:
   - `gl_shadowmap_pvs_priority`
   - `gl_shadowmap_hysteresis`
   - `gl_shadowmap_sticky_ms`
   - `gl_shadowmap_sticky_boost`
3. Expanded LRU cache residency path keyed by light hashes and separate cache
   slot assignment.

## Target behavior after rebuild
1. Build candidate shadow lights from visible shadow-capable dlights.
2. Score all candidates with a single deterministic function.
3. Select top N lights (`N = min(gl_shadowmap_lights, hardware cap)`).
4. Assign slots `0..N-1` directly to selected lights.
5. For static lights:
   - reuse cached faces only when slot is clean under selected cache mode.
   - otherwise rerender and refresh cache.
6. For non-selected lights:
   - do not inject fallback unshadowed static-light shading.

## Implementation phases
### Phase 1: Data model cleanup
- Remove stale shadow cache/LRU state and cvars tied to old slot churn system.
- Keep only shadow cache state required by mode 0/1/2.

### Phase 2: Selection rewrite
- Replace selection logic with deterministic score sort and direct slot assign.
- Keep candidate/debug counters coherent.

### Phase 3: Cache/render rewrite
- Remove indirection through separate cache-slot ownership.
- Make static cache checks and invalidation mode-driven and explicit.

### Phase 4: Shader upload contract cleanup
- Remove fallback-dependent static light upload allowance.
- Keep spot/pcss flags and shadow index upload intact.

### Phase 5: Verification
- Build with `meson compile -C builddir`.
- Validate expected behavior manually in-map:
  - static shadow stability.
  - dynamic caster invalidation in mode 1.
  - world-only reuse in mode 2.
  - no static no-slot fallback banding.

## Progress checklist
- [x] Plan documented.
- [x] Data model cleanup complete.
- [x] Selection rewrite complete.
- [x] Cache/render rewrite complete.
- [x] Shader contract cleanup complete.
- [x] Build verification complete.

## Implemented in this pass
### Renderer state + cvar cleanup
- Removed no-slot fallback and slot-churn tuning cvar registration from
  `src/rend_gl/main.c`:
  - `gl_shadowmap_cache_lights`
  - `gl_shadowlight_no_slot_mode`
  - `gl_shadowmap_pvs_priority`
  - `gl_shadowmap_hysteresis`
  - `gl_shadowmap_sticky_ms`
  - `gl_shadowmap_sticky_boost`
- Removed related declarations from `src/rend_gl/gl.h`.

### Deterministic shadow light selection
- Replaced `GL_SelectShadowLights()` with a deterministic score sort and direct
  slot mapping (`slot == selected rank`) in `src/rend_gl/main.c`.
- Removed prior sticky/hysteresis/LRU slot retention behavior.
- Retained PVS2 visibility gating and PCSS top-N selection control.

### Simplified cache model
- Replaced separate cache-slot ownership with direct per-shadow-slot cache use
  in `GL_RenderShadowMaps()` (`src/rend_gl/main.c`).
- Cache mode behavior now follows explicit baseline:
  - mode `0`: no cache path.
  - mode `1`: static slot invalidates when dynamic casters intersect light.
  - mode `2`: world-only static cache reuse (no dynamic invalidation).
- `GL_EnsureShadowmaps()` now allocates cache layers equal to active shadow
  layers (`src/rend_gl/main.c`).

### Cache metadata pruning
- Removed LRU/hash residency metadata from `glStatic_t` in `src/rend_gl/gl.h`.
- Removed stale initialization/shutdown resets in `src/rend_gl/texture.c`.

### Shader upload contract cleanup
- Removed fallback-dependent static-light upload allowances from:
  - `src/rend_gl/shader.c`
  - `src/rend_gl/sp3b.c`
- Static shadow-designated lights without slots are now consistently skipped.

## Verification
- Build check: `meson compile -C builddir` completed successfully.
- Manual in-map validation is still required for visual behavior tuning and
  confirmation.
