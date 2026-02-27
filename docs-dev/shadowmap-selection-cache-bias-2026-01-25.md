# Shadowmap Selection, Caching, and Bias Updates (2026-01-25)

## Overview
This update aligns shadow range with dynamic light attenuation, reduces per-frame CPU churn in shadow cache decisions, and stabilizes shadow light slot assignment. It also enforces `gl_shadow_pcss_max_lights`, applies bias scaling to sun shadows, and updates default bias cvars for more robust out-of-box results.

## Key Renderer Changes

### Shadow caster tracking and cache invalidation
- Added per-frame shadow-caster lists (all casters + dynamic casters) built during `GL_ClassifyEntities`.
- Cache invalidation now scans only dynamic casters for mode `gl_shadowmap_cache_mode 1` instead of all entities.
- Caster hashing uses the caster list (not the full entity list) and includes shadow-relevant toggles:
  - `gl_shadow_alpha_mode` value
  - `ent->alpha`
  - `RF_TRANSLUCENT`
- `gl_shadow_alpha_mode` now invalidates cached shadow layers on change.

### Light selection stability and visibility
- Shadow light candidates are filtered by view frustum and PVS/areabits before scoring.
- Slot assignment uses hysteresis to keep previous slots when scores are close, reducing churn and cache thrash.
- Scoring now modestly prefers lights in front of the camera to reduce off-screen churn.

### Shadow range alignment and spotlight culling
- Shadow map radius uses `dl->radius + DLIGHT_CUTOFF` to match shading attenuation.
- Shadowed point lights no longer apply the per-fragment `+ v_norm * 16` offset (kept for non-shadowed lights).
- Spotlight shadow passes now include conservative cone checks per entity using bounding sphere expansion.

### PCSS gating and bias scaling
- `gl_shadow_pcss_max_lights` is enforced per frame; only the top N selected lights use PCSS.
- Non-PCSS shadowed lights fall back to PCF when the global filter is PCSS.
- `gl_shadow_bias_scale` now applies to sun shadows as well as dynamic lights.

## Cvar Defaults Updated
- `gl_shadow_bias_slope`: `0.0` -> `1.0`
- `gl_shadow_normal_offset`: `0.0` -> `0.5`

## Notes on Behavior
- Shadow map cache updates are more stable because light slots persist across frames when scores are within the hysteresis window.
- Shadow/attenuation alignment reduces over-shadowing in the fade region.
- PCSS cost is bounded for multi-light scenes; remaining shadowed lights use PCF.

## Files Touched
- `src/rend_gl/main.c`
- `src/rend_gl/shader.c`
- `src/rend_gl/world.c`
- `src/rend_gl/gl.h`
- `src/rend_gl/sp2.c`
- `src/rend_gl/sp3b.c`
