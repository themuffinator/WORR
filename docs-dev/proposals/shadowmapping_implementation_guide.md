# Production-Quality Soft Shadow Mapping in Q2REPRO (idTech2)

This document is a **comprehensive implementation guide** for fixing, hardening, and extending the current shadow mapping system in a Q2REPRO-based renderer to a **correct, efficient, and production-quality** standard.

It is written specifically for:
- idTech2 BSP quirks
- Q2REPRO’s GLSL/ARB pipeline
- Mixed **static map/entity lights** and **dynamic effect lights**
- Fully user-configurable quality (PCF, VSM, EVSM, PCSS, CSM)

---

## 1. Core Constraints of idTech2 (What You Must Respect)

### 1.1 BSP, PVS, and Why Shadows Break
Quake II rendering is built around:
- BSP tree
- Leaf clusters
- PVS (Potentially Visible Set)

**Critical reality:**  
Shadow maps treat the *light as a camera*. If you reuse camera-centric logic:
- Lights inside solid leaves can yield empty PVS
- World geometry may not be drawn into the shadow map at all
- Shadow behavior becomes unstable or “random”

**Rule:**  
> Shadow rendering must *not* depend on camera PVS logic.

---

### 1.2 Light Placement Reality
Map lights in Quake II:
- Are often placed *inside walls, fixtures, or brushes*
- Were never intended to be “render viewpoints”
- Work fine for lightmaps, but not for shadow cameras

You **must** compensate for this.

---

## 2. High-Level Architecture (Target Design)

### 2.1 Separate Shadow System Concerns

| Concern | Must Be Independent |
|------|---------------------|
| Shadow caster selection | Camera visibility |
| World occluder selection | PVS |
| Static vs dynamic lights | Update logic |
| Shadow quality | Light type |

**Never reuse camera render lists for shadow passes.**

---

### 2.2 Shadow-Capable Light Types
All lights participate equally:
- Map/entity lights (`light`, `dynamic_light`)
- Effect lights (projectiles, explosions)
- Optional directional/sun light (CSM)

Light importance is decided by **priority**, not by type.

---

## 3. Phase 1 – Fix Correctness (Non-Negotiable)

### 3.1 Fix Brush Model (BModel) Shadow Culling

#### Problem
`func_rotating`, doors, and other inline BSP models:
- Use `ent->origin` as a pivot
- Are **not centered geometrically**
- Are incorrectly culled from shadow casting

#### Fix
For inline BSP models:
1. Use model bounds (`mins`, `maxs`)
2. Compute local center
3. Transform to world space
4. Use this for range tests

```c
center_local = (mins + maxs) * 0.5;
center_world = ent->origin + center_local;
```

Use this center for:
- Light range tests
- Shadow caster inclusion

---

### 3.2 Build a Dedicated Shadow Caster List

#### Problem
Shadow casters are currently pulled from:
- Camera-visible render entities
- Which excludes:
  - Local player (1st person)
  - Culled monsters
  - Hidden but shadow-relevant entities

#### Required Change
Create a **shadow-caster list** per frame.

Include:
- All entities with models in the current snapshot
- Brush models
- Player model (even in first-person, shadow-only)
- Exclude:
  - Beams
  - Particles
  - RF_NOSHADOW entities

This list is independent of:
- Frustum culling
- LOD
- Camera visibility

---

### 3.3 Fix Static Shadow Map Caching (Major Bug)

#### Problem
Static lights reuse cached shadow maps even when:
- Dynamic entities move underneath them
- BModels rotate or animate

This causes:
- Frozen shadows
- Missing player/monster shadows

#### Correct Caching Modes
Introduce:

```
gl_shadowmap_cache_mode
```

| Mode | Behavior |
|---|---|
| 0 | No caching (always update) |
| 1 | Cache only if no dynamic casters intersect |
| 2 | Cache world-only occluders (advanced) |

**Mode 1 is required for correctness.**

Logic:
- If any dynamic caster intersects light radius → invalidate cache
- Otherwise reuse shadow map

---

### 3.4 Light Origin Safety (Lights Inside Solid)

#### Problem
Shadow cameras placed inside solid BSP leaves:
- Fail world rendering
- Produce empty or partial shadow maps

#### Fix: Shadow-Safe Origin
When creating shadow views:
1. Test `CM_PointContents(light.origin)`
2. If solid:
   - For spotlights: push along cone direction
   - For point lights: test small offset directions
3. Use first non-solid position

Store:
- `light->origin_render`
- `light->origin_shadow`

Always use `origin_shadow` for:
- Shadow map rendering
- Depth comparisons

---

### 3.5 World Occluder Selection (No PVS)

#### Problem
Using PVS-from-light-position causes:
- Skipped world geometry
- Missing shadows

#### Correct Method
Traverse BSP using **light volume intersection**:

```
R_MarkShadowSurfaces(light):
    recurse BSP
    test node bounds vs light sphere/cone
    mark intersecting surfaces
```

Render only marked surfaces into the shadow map.

This mirrors how Quake II marks dlight-influenced surfaces.

---

## 4. Phase 2 – Light Selection & Fairness

### 4.1 Unified Light Priority System

Do not prioritize static lights over dynamic ones.

Compute a per-light score:
- Intensity / brightness
- Distance to camera
- Screen-space influence
- Optional boost for dynamic lights

Sort all shadow-capable lights by score.

Select:
```
gl_shadowmap_lights_max
```

Optional guarantees:
- `gl_shadowmap_lights_min_dynamic`
- `gl_shadowmap_lights_min_static`

---

## 5. Phase 3 – Shadow Filtering & Bias (Quality)

### 5.1 Bias Model (Mandatory)

Replace constant bias with:
- Constant bias
- Slope-scaled bias
- Normal offset

Cvars:
```
gl_shadow_bias_const
gl_shadow_bias_slope
gl_shadow_normal_offset
```

Apply:
- Normal offset in receiver space
- Slope bias based on `dot(N, L)`

This eliminates acne and reduces peter-panning.

---

### 5.2 PCF (Baseline Soft Shadows)

Support:
- Box filters (2x2, 3x3, 4x4)
- Poisson disk sampling (8–16 taps)
- Per-pixel kernel rotation (noise or hash)

Cvars:
```
gl_shadow_filter = hard|pcf
gl_shadow_pcf_taps
gl_shadow_softness
```

---

### 5.3 VSM / EVSM (Filtered Soft Shadows)

#### Required Changes
When using VSM/EVSM:
- Use `GL_LINEAR` filtering
- Enable mipmaps or blur passes
- Store depth moments properly

Cvars:
```
gl_shadow_filter = vsm|evsm
gl_shadow_vsm_bleed_reduction
gl_shadow_vsm_mipmaps
```

EVSM is recommended for large softness ranges.

---

### 5.4 PCSS (Contact-Hardening)

Optional high-end path:
- Blocker search (PCF)
- Penumbra size estimation
- Variable-radius PCF

Cvars:
```
gl_shadow_filter = pcss
gl_shadow_pcss_blocker_samples
gl_shadow_pcss_filter_samples
```

---

## 6. Phase 4 – Directional Light & Cascaded Shadows

### 6.1 Separate Directional Shadow System
Do **not** shoehorn sun shadows into point/spot logic.

Use:
- Cascaded Shadow Maps (CSM)
- 2D texture array
- Orthographic projections per cascade

Cvars:
```
gl_sun_enable
gl_sun_dir
gl_csm_cascades
gl_csm_lambda
gl_csm_resolution
```

Use stabilized cascade snapping to avoid shimmering.

All filtering options (PCF/VSM/EVSM) apply to cascades.

---

## 7. Debugging & Validation Tools (Strongly Recommended)

Add:
- Shadow map visualization mode
- Per-light shadow frustum debug
- Caster count per light
- Cache invalidation debug

Cvars:
```
gl_shadow_debug
gl_shadow_debug_light
gl_shadow_debug_freeze
```

---

## 8. Minimum Checklist for “Production-Correct”

You are **not production-ready** unless all are true:

- [ ] BModels cast shadows correctly
- [ ] Player/monsters cast shadows even when camera-culled
- [ ] Static lights update when dynamic casters move
- [ ] Lights inside geometry still shadow correctly
- [ ] World geometry never disappears from shadow maps
- [ ] Dynamic lights reliably cast shadows
- [ ] Filtering quality matches user-selected method

---

## 9. Final Notes

idTech2 was never designed for real-time shadow mapping.  
A correct implementation requires **architectural separation**, not shader tricks.

If correctness is solved first:
- PCF, VSM, EVSM, PCSS, and CSM become *clean upgrades*
- Not compensations for broken visibility

This guide intentionally prioritizes:
> **Correctness → Stability → Performance → Quality**

in that order.
