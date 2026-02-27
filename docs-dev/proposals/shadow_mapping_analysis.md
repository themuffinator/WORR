# Shadow Mapping Implementation Analysis & Proposal

**Date:** 2026-01-24  
**Subject:** Scrutiny of GL Shadow Mapping & Dlights  
**Status:** Updated with peer-reviewed research findings and code-level verification

## 1. Executive Summary

The current shadow mapping implementation in `rend_gl` is feature-rich, supporting modern techniques like Cascaded Shadow Maps (CSM), Percentage-Closer Filtering (PCF), Percentage-Closer Soft Shadows (PCSS), Variance Shadow Maps (VSM), and Exponential VSM (EVSM). It integrates cleanly with the idTech2-style forward renderer by treating shadow maps as texture arrays and performing lighting/shadow logic in the fragment shader.

However, several inefficiencies and gaps remain: CPU-side light selection and cache invalidation are O(Lights × Entities) and lack PVS/frustum pruning; slot instability can churn the cache; and the cache strategy adds VRAM bandwidth via per-layer copies. There are also correctness/quality mismatches (attenuation vs shadow range, bias defaults and bias scaling only applied to dynamic lights) and completeness gaps (`gl_shadow_pcss_max_lights` is unused; alpha shadow modes are incomplete). These are fixable within renderer-only changes and should preserve legacy protocol/demos.

---

## 2. Peer-Reviewed Research Context

The following peer-reviewed sources informed this analysis (industry/technical references called out separately):

| Technique | Peer-Reviewed Reference | Notes |
|-----------|-------------------------|-------|
| **Shadow Maps / PCF** | Reeves, Salesin, Cook (SIGGRAPH 1987) "Rendering Antialiased Shadows with Depth Maps" | Foundation of depth-map shadows and PCF-style filtering |
| **VSM** | Donnelly & Lauritzen (I3D 2006) "Variance Shadow Maps" | Moment-based filtering; light-bleeding tradeoffs |
| **ESM** | Annen et al. (Graphics Interface 2008) "Exponential Shadow Maps" | Exponential warp reduces bleeding; requires exponent clamping |
| **PSSM/CSM** | Zhang et al. (2006) "Parallel-Split Shadow Maps for Large-scale Virtual Environments" | Log/linear split for perspective aliasing; basis for CSM |

Industry/technical references used for implementation guidance (non-peer-reviewed):
- Fernando (2005) PCSS whitepaper (blocker search + penumbra estimation)
- GPU Gems 3 (VSM/CSM/PCF implementation notes)
- Microsoft DirectX/OpenGL bias guidelines, LearnOpenGL summaries

### Quake II Rerelease (public reference, unverified)

- Public builds expose a Dynamic Shadows toggle and a `r_staticshadows` console cvar (community-reported). Without renderer source access, treat this as a black-box reference and validate parity against the shipping binary rather than assume algorithmic details.

### Key Takeaways from Literature

1. **VSM Light Bleeding**: VSM suffers from light bleeding when multiple occluders exist at different depths (Donnelly & Lauritzen, 2006). The EVSM technique mitigates this but requires careful tuning of the exponent to avoid overflow. The current implementation clamps at `exp(80)` and uses a default exponent of `30.0`; bleed/min-variance defaults are derived from `gl_shadowmap_quality`, with optional overrides via `gl_shadow_vsm_bleed` and `gl_shadow_vsm_min_variance`.

2. **Bias Tradeoffs**: Slope-scale bias and normal-offset bias remain standard remedies for **shadow acne** and **peter panning**. The renderer exposes both via `gl_shadow_bias_slope` and `gl_shadow_normal_offset`, but defaults are `0.0` and bias scaling (`gl_shadow_bias_scale`) only applies to dynamic lights. Non-zero defaults and consistent scaling across sun/dlights are recommended for robust scenes.

3. **Cube Map Emulation**: The manual cube-to-2D-array projection in `shadow_cube_uv()` is mathematically correct and matches the standard cube-shadow approach. `GL_CLAMP_TO_EDGE` is correctly used to minimize edge seams, though grazing-angle artifacts remain possible.

4. **CSM Cascade Stability**: The implementation follows PSSM-style logarithmic/linear splits and includes cascade blending. It also snaps cascade centers to shadow-map texels, which is a known stabilization technique to reduce shimmering during camera motion.

5. **PCSS Implementation**: The three-stage PCSS algorithm (blocker search → penumbra estimation → filtering) matches Fernando's formulation. The sample budget is controlled by cvars (`gl_shadow_pcss_blocker_samples`, `gl_shadow_pcss_filter_samples`) and clamped to 16 Poisson offsets with a per-fragment rotation; this is efficient but can introduce temporal noise.

6. **BSP/PVS Integration**: IdTech2's BSP/PVS data provides O(1) visibility tests for clusters. The shadow pass currently uses sphere/box marking rather than PVS, making PVS-based light culling a low-risk optimization.

---

## 3. Detailed Analysis

### 3.0. Implementation Snapshot (Current Code)

- Shadow maps use 2D array color textures (preferred `GL_RG16F`, fallbacks RG16/RG8/RGBA) with a depth renderbuffer; shadow depth is written to color (`depth`, `depth^2` for VSM, warped moments for EVSM).
- Local light depth uses `dist * (1/radius)`; sun shadows use `gl_FragCoord.z` with orthographic projection.
- Filter modes: `gl_shadowmap_filter` `0..4` = hard, PCF, VSM, EVSM, PCSS (default `2` = VSM). `gl_shadowmap_quality` `0..3` drives PCF kernel size and VSM bleed/min-variance defaults.
- VSM/EVSM sampling uses linear filtering; optional mipmaps (`gl_shadow_vsm_mipmaps 1`) apply LOD bias tied to `gl_shadowmap_softness`.
- Bias controls: `gl_shadowmap_bias` default `0.003`, `gl_shadow_bias_slope` and `gl_shadow_normal_offset` default `0.0`. `gl_shadow_bias_scale` defaults to `1.0` and is applied only for dynamic lights.
- PCSS uses 16 Poisson offsets; blocker/filter counts default `8/16` and clamp to `16`. `gl_shadow_pcss_max_lights` is defined but unused.
- CSM uses `gl_csm_lambda` splits, `gl_csm_blend` fade, and texel snapping to stabilize cascades.
- Lights embedded in solid are shifted using `GL_ShadowmapFindSafeOrigin` (offset search along cone/axes).

### 3.1. Correctness

| Aspect | Status | Notes |
|--------|--------|-------|
| **Shadow Projection & Depth Encoding** | ✅ Correct | `shadow_cube_uv()` maps direction to face+UV; local lights encode `dist/radius`, sun shadows use `gl_FragCoord.z` |
| **CSM Splitting & Stabilization** | ✅ Correct | Log/linear PSSM splits with cascade blending and texel snapping in `GL_UpdateSunShadowData()` |
| **Light Origin Safety** | ✅ Correct | `GL_ShadowmapFindSafeOrigin()` shifts lights embedded in solid to avoid invalid captures |
| **Texture Clamping** | ✅ Correct | `GL_CLAMP_TO_EDGE` applied to all shadow map textures ([texture.c:1836-1837](../../src/rend_gl/texture.c#L1836-L1837)) |
| **Bias Controls & Scaling** | ⚠️ Mixed | Slope/normal-offset defaults are `0.0`, and `gl_shadow_bias_scale` is applied only to dynamic lights |
| **Attenuation vs Shadow Range** | ⚠️ Mismatch | Lighting uses `radius + DLIGHT_CUTOFF`, but shadow depth uses `radius` (fade region can be over-shadowed) |
| **Alpha Shadowing** | ⚠️ Incomplete | `gl_shadow_alpha_mode` has a TODO for mode 2; translucent casters are coarse/noisy |
| **Barriers/Sync** | ⚠️ Implicit | Relies on driver synchronization for `qglCopyTexSubImage3D`; generally safe but worth documenting |

### 3.2. Efficiency

#### CPU Overhead

| Function | Issue | Complexity |
|----------|-------|------------|
| `GL_SelectShadowLights()` | Scores all dlights; no PVS/frustum culling; slot churn | O(NumDlights × NumShadowSlots) |
| `GL_ShadowmapLightHasCasters()` | Iterates ALL entities per light | O(NumShadowLights × NumEntities) |
| `GL_ShadowmapCasterHash()` | Iterates ALL entities per light | O(NumShadowLights × NumEntities) |
| `GL_DrawShadowEntities()` | Iterates entities per light/face; no cone culling | O(NumShadowLights × Faces × NumEntities) |

> [!WARNING]
> For 16 shadow lights and 1000 entities, cache checks alone produce **16,000+ bounds checks per frame**, plus per-face entity filtering during shadow rendering.

#### GPU Overhead

| Aspect | Cost | Notes |
|--------|------|-------|
| **Cache VRAM Copies** | Medium | `GL_CopyShadowmapLayer()` copies cached faces to active texture every frame |
| **PCSS Sample Loops** | Low-Medium | Up to 16 samples per stage; sample counts are cvar-driven but loops may unroll |
| **CSM Multi-Pass** | High | Re-traverses world + entities for EACH cascade (4 passes typical) |
| **Spotlights in 6-Layer Arrays** | Low-Medium | Spotlights render 1 face but still reserve 6 layers (wasted layers/bandwidth) |
| **Mipmap Generation** | Low | Only for VSM/EVSM modes |

### 3.3. Quality

| Aspect | Quality | Notes |
|--------|---------|-------|
| **Filtering Options** | ⭐⭐⭐⭐⭐ | PCF, VSM, EVSM, PCSS all supported via `gl_shadowmap_filter` |
| **Light Bleeding Control** | ⭐⭐⭐ | VSM/EVSM include `bleed`/`min_var`; EVSM is 2-moment with positive exponent only |
| **Temporal Stability** | ⭐⭐⭐ | PCSS rotates samples per fragment (noise); light-slot churn can pop |
| **Cascade Blending** | ⭐⭐⭐⭐⭐ | Smooth transitions prevent visible cascade boundaries |
| **Depth Bias & Offset** | ⭐⭐⭐ | Cvars exist but defaults are `0.0`; bias scale is not applied to sun shadows |
| **Cube Seams** | ⭐⭐⭐⭐ | Clamping is correct; minor seams possible at extreme angles |

---

## 4. Issues & Recommendations

### Issue 1: O(N×M) CPU Culling for Shadow Cache

**Severity:** 🔴 High (Performance)  
**Location:** [main.c:2474-2519](../../src/rend_gl/main.c#L2474-L2519) → `GL_ShadowmapLightHasCasters()`, `GL_ShadowmapCasterHash()`  
**Description:** Cache invalidation and caster hashing iterate the entire entity list for each shadow-casting light, and do not account for alpha/visibility toggles that can affect casters.

**Recommendations:**
1. **Immediate Fix:** Track a dynamic-caster list (or per-light list) and iterate only entities that can move/animate/change (frame/oldframe/backlerp, non-identity scale, `RF_TRANSLUCENT`) when invalidating cache for static lights.
2. **Hash Hygiene:** Include shadow-relevant toggles (alpha mode, `ent->alpha`, `RF_TRANSLUCENT`) in the caster hash to avoid stale cached shadows.
3. **Spatial Acceleration:** Reuse BSP leaf lists or a grid to query entities within a light radius instead of full scans.
4. **Optional:** Offload "has casters" to GPU via occlusion queries if CPU remains hot.

---

### Issue 2: Light Selection Lacks PVS/Frustum and Slot Stability

**Severity:** 🟡 Medium (Performance/Quality)  
**Location:** [main.c:2555-2616](../../src/rend_gl/main.c#L2555-L2616) → `GL_SelectShadowLights()`  
**Description:** Lights are scored without PVS/frustum rejection, and slots are re-assigned each frame based solely on score, which can cause popping and cache churn.

**Recommendations:**
1. **Visibility Culling:** Apply PVS and view-frustum tests before scoring candidates.
2. **Stable Assignment:** Preserve previous-frame slots with a hysteresis threshold to avoid churn.
3. **Scoring Refinement:** Incorporate view direction or distance fade to de-prioritize off-screen lights.

---

### Issue 3: Shadow Cache Bandwidth (VRAM Copies)

**Severity:** 🟡 Medium (Efficiency)  
**Location:** [main.c:2631-2641](../../src/rend_gl/main.c#L2631-L2641) → `GL_CopyShadowmapLayer()`  
**Description:** Every frame, cached shadow faces are copied from cache texture to active texture via `qglCopyTexSubImage3D`. `shadowmap_cache_scene_hash` and `shadowmap_cache_scene_valid` are currently unused.

**Recommendations:**
1. **Unified Array:** Use a single persistent texture array and stable slots to eliminate per-frame copies.
2. **LRU Eviction:** Add LRU slot eviction if VRAM is constrained.
3. **Alternative:** Sample directly from cached + live textures with a per-light flag if dual textures are retained.

---

### Issue 4: Attenuation vs Shadow Range Mismatch

**Severity:** 🟡 Medium (Correctness/Quality)  
**Location:** [shader.c:623-666](../../src/rend_gl/shader.c#L623-L666), [main.c:1852-1857](../../src/rend_gl/main.c#L1852-L1857)  
**Description:** Lighting uses `radius + DLIGHT_CUTOFF` and offsets non-spot light position by `v_norm * 16`, while shadow depth uses the base light origin and `radius`. This can over-shadow the fade region and misalign lighting vs shadowing.

**Recommendations:**
1. **Range Alignment:** Use a consistent radius for both shading and shadow depth (adopt `radius + DLIGHT_CUTOFF` in shadow depth, or shrink attenuation when shadowing is enabled).
2. **Position Consistency:** Remove the non-spot `light_pos` offset or apply the same offset during shadow map rendering (or treat it as bias/normal offset instead).
3. **Spotlight Culling:** Add cone-based culling for shadow pass to reduce extra draws.

---

### Issue 5: Bias Defaults and Sun Bias Scaling

**Severity:** 🟢 Low (Quality)  
**Location:** [main.c:3522-3556](../../src/rend_gl/main.c#L3522-L3556), [shader.c:623-669](../../src/rend_gl/shader.c#L623-L669)  
**Description:** Slope/normal-offset defaults are `0.0`, and `gl_shadow_bias_scale` only scales dynamic lights (sun shadows use unscaled bias).

**Recommendations:**
1. **Defaults:** Ship non-zero slope/normal-offset defaults tuned for idTech2 scale (start around `1.0` and `0.5`, then tune).
2. **Sun Scaling:** Apply `gl_shadow_bias_scale` to sun shadows or add `gl_shadow_bias_scale_sun` for independent control.

---

### Issue 6: PCSS Control Gaps (`gl_shadow_pcss_max_lights` Unused)

**Severity:** 🟡 Medium (Scalability)  
**Location:** [main.c:3555-3556](../../src/rend_gl/main.c#L3555-L3556)  
**Description:** `gl_shadow_pcss_max_lights` exists but is never enforced, so PCSS can be applied to all selected lights, causing heavy GPU cost.

**Recommendations:**
1. **Enforce Limit:** Apply PCSS only to the top N lights and fall back to PCF/EVSM for the rest.
2. **Expose Debugging:** Add counters or debug overlays to verify which lights use PCSS.

---

### Issue 7: CSM Redundant Draw Calls

**Severity:** 🟡 Medium (CPU/Driver)  
**Location:** [main.c:2643-2702](../../src/rend_gl/main.c#L2643-L2702) → `GL_RenderSunShadowMaps()`  
**Description:** World and entity lists are re-traversed for each cascade (typically 4 passes).

**Recommendations:**
1. **Layered Rendering:** Use `GL_ARB_shader_viewport_layer_array` to render all cascades in a single pass (complex to retrofit into idTech2).
2. **Tighter Culling:** Implement per-cascade frustum/leaf culling to reduce redundant submissions.

---

### Issue 8: Shadow Caster Coverage & Alpha Modes

**Severity:** 🟢 Low (Completeness)  
**Location:** [main.c:924-936](../../src/rend_gl/main.c#L924-L936), [main.c:2314-2360](../../src/rend_gl/main.c#L2314-L2360)  
**Description:** Shadow casting is limited to alias models and bmodels; translucent casters are treated as either fully on/off, and `gl_shadow_alpha_mode` has a TODO for mode 2.

**Recommendations:**
1. **Alpha Shadows:** Implement mode 2 with alpha-tested cutouts or weighted opacity.
2. **Model Coverage:** If MD5/other formats are in use, extend `GL_EntityCastsShadowmap()` to include them.
3. **Documentation:** Explicitly document caster limitations for content authors.

---
## 5. Proposed Implementation Plan

Renderer-only changes; no protocol or demo compatibility impact.

### Phase 1: High-Priority Optimizations

| # | Task | Impact | Effort |
|---|------|--------|--------|
| 1.1 | Replace O(N×M) cache invalidation with dynamic-caster tracking + hash hygiene | 🔴 High | 🟡 Medium |
| 1.2 | Add PVS/frustum culling and stable slot assignment (hysteresis) in `GL_SelectShadowLights()` | 🟡 Medium | 🟡 Medium |
| 1.3 | Align attenuation/shadow range and remove non-spot light position mismatch | 🟡 Medium | 🟡 Medium |
| 1.4 | Enforce `gl_shadow_pcss_max_lights` and fall back to PCF/EVSM | 🟡 Medium | 🟢 Low |
| 1.5 | Set non-zero bias defaults and apply a sun bias scale | 🟢 Low | 🟢 Low |

### Phase 2: Architecture Improvements

| # | Task | Impact | Effort |
|---|------|--------|--------|
| 2.1 | Unified persistent shadow array (eliminate cache copies) | 🟡 Medium | 🟡 Medium |
| 2.2 | LRU slot eviction + stable slot IDs for cached lights | 🟢 Low | 🟡 Medium |
| 2.3 | Tighter shadow-pass culling (per-cascade frustum/leaf, spotlight cone) | 🟡 Medium | 🟡 Medium |

### Phase 3: Quality/Completeness

| # | Task | Impact | Effort |
|---|------|--------|--------|
| 3.1 | Implement alpha shadow mode 2 (alpha-tested/weighted) | 🟢 Low | 🟡 Medium |
| 3.2 | Add ESM or full 4-moment EVSM option | 🟢 Low | 🟢 Low |
| 3.3 | Extend caster coverage (MD5/other formats) if enabled; document limits | 🟢 Low | 🟡 Medium |

---
## 6. Verification Plan

### Automated Testing
- **Shadow map rendering**: Run existing visual regression tests (if available) or verify on `base1` with `gl_shadowmap_filter`, `gl_shadow_vsm_mipmaps`, and `gl_shadow_pcss_*` combinations.
- **Performance**: Benchmark with `r_speeds` and add counters for light selection and cache invalidation loops.
- **Cache behavior**: Toggle `gl_shadowmap_cache` and `gl_shadowmap_cache_mode`; validate `gl_shadow_pcss_max_lights` gating and alpha-mode changes.

### Manual Verification
1. Load a complex map with many dynamic lights and entities.
2. Enable shadowmaps: `gl_shadowmaps 1` and rotate the camera to check light-slot stability.
3. Inspect the fade region of a dlight to confirm attenuation and shadow range alignment.
4. Verify no shadow acne or excessive peter panning with default bias values.
5. Validate smooth cascade transitions for sun shadows.

---
## 7. Conclusion

The renderer is advanced and functionally complete, implementing PCF, VSM, EVSM, PCSS, and CSM with stabilized cascades and correct cube-map projection. The design aligns with peer-reviewed foundations while respecting idTech2 constraints (BSP, MD2).

The largest practical gaps are CPU-side: cache invalidation still scans all entities per light, light selection lacks PVS/frustum pruning and stability, and the cache copy path burns VRAM bandwidth. Correctness/quality issues are mostly parameterization and consistency: attenuation vs shadow range mismatch, bias defaults/scaling for sun, and incomplete alpha handling.

The updated plan focuses on renderer-only improvements (no protocol/demo changes) that deliver the biggest wins first, then refines architecture and quality with controlled risk. This keeps WORR compatible with legacy servers/demos while moving shadow mapping closer to Quake II Rerelease parity.
