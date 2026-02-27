# Shadowmap Stability, Fallback, and Cache Expansion (2026-02-16)

## Goal
Implement the shadowlight popping/artifact mitigation plan from the deep research report by:
- reducing shadow slot churn effects,
- preventing banded unshadowed fallback artifacts,
- keeping strict-PVS lights prioritized,
- exposing independent dlight vs shadow-slot pressure in HUD/debug stats,
- expanding static shadow cache residency beyond active shadow slots.

## Implemented Changes

### 1) No-slot static shadowlight fallback mode (strict-PVS only)
- Added `gl_shadowlight_no_slot_mode`:
  - `0` (default): keep current safe behavior, skip static shadowlights (`DL_SHADOW_LIGHT`) without a shadow slot.
  - `1`: allow unshadowed fallback only when the light is in strict view PVS.
- Added per-dlight selection metadata in `glRefdef_t`:
  - `shadowmap_vis[]`
  - `shadowmap_fallback[]`
  - `shadowmap_cache_slot[]`
- `GL_SelectShadowLights()` now marks strict-PVS no-slot static lights as fallback-eligible when mode `1` is enabled.
- Shader upload (`shader_load_lights` in both GL shader upload paths) now allows no-slot static lights only when fallback was explicitly marked.

Files:
- `src/rend_gl/main.c`
- `src/rend_gl/shader.c`
- `src/rend_gl/sp3b.c`
- `src/rend_gl/gl.h`

### 2) Banded artifact guard for unshadowed point-light fallback
- Replaced unconditional shader-side `light_pos += v_norm * 16` behavior with a gated flag (`shadow.w`).
- Added `gl_shadow_unshadowed_point_offset`:
  - `0` (default): disable legacy offset (recommended for artifact suppression).
  - `1`: re-enable legacy offset behavior for non-fallback unshadowed point lights.
- GLSL generation updated in both shader writers so offset happens only when `shadow.w > 0.5`.

Files:
- `src/rend_gl/shader.c`
- `src/rend_gl/sp2.c`
- `src/rend_gl/sp3b.c`

### 3) Strict-PVS priority + stronger temporal stability
- Added strict-PVS score weighting in `GL_SelectShadowLights()`:
  - `gl_shadowmap_pvs_priority` (default `1.35`).
- Replaced fixed hysteresis constant with cvar:
  - `gl_shadowmap_hysteresis` (default `0.92`).
- Added time-based sticky score boost for recently selected lights:
  - `gl_shadowmap_sticky_ms` (default `250` ms),
  - `gl_shadowmap_sticky_boost` (default `1.25`).

Files:
- `src/rend_gl/main.c`

### 4) Expanded static-light cache residency (separate cache capacity)
- Added separate cache capacity control:
  - `gl_shadowmap_cache_lights` (default `8`), clamped by array texture layer limits.
- Shadow cache texture is now allocated with its own layer count (`cache_layers`) separate from active shadowmap layers.
- Added cache key + LRU assignment for static lights:
  - static lights acquire persistent cache slots by hashed light identity,
  - cache survives active slot reassignment churn,
  - selected active slots can copy from persistent cache slots.
- Cache metadata arrays expanded from `MAX_SHADOWMAP_LIGHTS` to `MAX_SHADOWMAP_CACHE_LIGHTS` (`32`).

Files:
- `src/rend_gl/texture.c`
- `src/rend_gl/main.c`
- `src/rend_gl/gl.h`

### 5) Debug/HUD visibility for independent caps and no-slot behavior
- Added renderer debug cvars updated during shadow debug pass:
  - `gl_shadow_debug_dlight_cap`
  - `gl_shadow_debug_shadow_candidates`
  - `gl_shadow_debug_shadow_slot_cap`
  - `gl_shadow_debug_shadow_requested_cap`
  - `gl_shadow_debug_shadow_skipped_no_slot`
  - `gl_shadow_debug_shadow_fallback_no_slot`
- Existing debug text/log/dump now include slot pressure and no-slot skip/fallback counts.
- FPS shadow debug overlay in cgame now displays:
  - dlights used/cap,
  - selected shadowlights,
  - candidate count,
  - requested/actual shadow slot cap,
  - no-slot skip and fallback counts,
  - caster counts.

Files:
- `src/rend_gl/main.c`
- `src/rend_gl/gl.h`
- `src/game/cgame/cg_draw.cpp`

## Existing Behavior Verified (No New Patch Needed)
- Method-aware shadow texture sampling already uses linear filtering for VSM/EVSM paths and nearest for compare/PCF paths via `GL_UpdateShadowmapSampling()`.

Files reviewed:
- `src/rend_gl/main.c`

## Build Verification
- `meson compile -C builddir`
- Result: success; updated OpenGL renderer and cgame DLLs built and linked cleanly.
