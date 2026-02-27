# Vulkan Particles Fix + GL Parity Audit (2026-02-11)

## Scope
- Fix missing projectile-trail particles in native Vulkan renderer (`worr_vulkan_x86_64.dll`).
- Audit parity gaps against OpenGL renderer and list concrete implementation work.

## 1) Projectile-Trail Particle Fix

### Symptom
- Projectile trails would partially disappear or vanish too aggressively in Vulkan.

### Root Cause
- Entity fragment shader discarded fragments at `base.a <= 0.01` before per-particle alpha modulation.
- For minified particle quads and low-alpha trail states, this culled too many fragments compared with GL behavior.

### Change
- Reduced discard gate to only reject fully transparent texels.
- Files:
  - `.codex_temp/vk_entity.frag`
  - `src/rend_vk/vk_entity_spv.h`

### Why this matches GL better
- GL particle path uses blend + particle alpha modulation and does not apply this early 0.01 fragment kill threshold.
- Keeping only zero-alpha discard preserves edge rejection while avoiding premature trail loss.

### Build Validation
- Rebuilt Vulkan renderer target successfully:
  - `ninja -C builddir worr_vulkan_x86_64.dll`

## 2) GL vs Vulkan Parity Audit

Legend:
- `OK`: Implemented and functionally aligned enough for current baseline.
- `PARTIAL`: Present but behavior diverges or has known defects.
- `MISSING`: Not implemented in Vulkan renderer yet.

## 2.1 Frame/Pass Architecture
- World opaque + alpha passes:
  - GL: `OK`
  - VK: `OK` (basic two-pass draw model)
- Transparent ordering parity (`gl_draworder` style behavior):
  - GL: `OK` (`gl_draworder`, split alpha lists)
  - VK: `PARTIAL` (batch-order driven; no depth-sorted transparent entity list parity)

## 2.2 World Surface Shading
- Base texture + lightmap combine:
  - GL: `OK`
  - VK: `OK`
- Alpha-test world surfaces:
  - GL: `OK`
  - VK: `PARTIAL` (threshold path present; still needs validation for all cutout content)
- Warp/flowing animation:
  - GL: `OK` (shader path with refraction options)
  - VK: `PARTIAL` (CPU UV warping path; no equivalent refraction pipeline)
- Dynamic light contribution over world:
  - GL: `OK`
  - VK: `PARTIAL` (present but simplified vs GL shader feature set)

## 2.3 Sky
- Skybox loading and draw:
  - GL: `OK`
  - VK: `PARTIAL` (implemented; prior visual artifacts indicate mapping/edge parity still not complete)
- Sky effects parity (advanced blending/debug modes):
  - GL: `PARTIAL/OK` depending on cvar path
  - VK: `PARTIAL`

## 2.4 Entities/Models
- MD2 rendering:
  - GL: `OK`
  - VK: `PARTIAL` (known user-reported correctness issues remain)
- MD5 replacement + animation interpolation:
  - GL: `OK` (where enabled)
  - VK: `PARTIAL` (interpolation path exists but still needs visual parity pass)
- Inline BSP model (bmodel) movement/state correctness:
  - GL: `OK`
  - VK: `PARTIAL` (movement updates improved, initial-state artifacts still reported)
- View weapon depth-hack layering:
  - GL: `OK`
  - VK: `PARTIAL` (implemented depthhack pass; requires final visual parity checks)

## 2.5 Particles/Beams/Flares
- Particles base rendering:
  - GL: `OK`
  - VK: `PARTIAL` (trail discard issue fixed in this change; more coverage still needed)
- Particle style parity (`gl_partstyle` additive mode):
  - GL: `OK`
  - VK: `MISSING` (no equivalent mode/cvar in Vulkan path)
- Beam style parity (`gl_beamstyle`):
  - GL: `OK`
  - VK: `MISSING` (no Vulkan equivalent style path)
- Flare entities (`RF_FLARE` behavior):
  - GL: `OK`
  - VK: `MISSING` (no explicit `RF_FLARE` path)

## 2.6 Feature Flags / Entity Behavior
- `RF_TRANSLUCENT`, `RF_FULLBRIGHT`, `RF_MINLIGHT`, depthhack weapon model:
  - GL: `OK`
  - VK: `OK/PARTIAL` (implemented core subset)
- `RF_NOSHADOW`, flare-specific behavior and GL-side exclusions:
  - GL: `OK`
  - VK: `MISSING/PARTIAL`

## 2.7 Post-Processing and Advanced Visual Features
- Bloom/HDR/fog/color grading/CRT/DOF class features:
  - GL: `OK`
  - VK: `MISSING` in native Vulkan raster path
- GL shadowmapping stack (`gl_shadowmap_*`, sun/cascade tuning):
  - GL: `OK`
  - VK: `MISSING` in native Vulkan raster path

## 3) CPU Footprint Findings (Vulkan)

Current high-cost hotspots:
- World dynamic vertex updates run CPU-side over world vertices and memcpy host-visible buffer when dynamic effects are active.
- Entity path emits fully expanded triangles on CPU each frame, then uploads monolithic vertex buffer.
- Frequent host-visible/coherent writes reduce GPU-side reuse opportunity.

Recommended work for minimal CPU footprint:
1. Move static world geometry into device-local immutable buffers, split dynamic attributes (color/warp terms) into compact dynamic stream.
2. Move warp/light modulation to GPU-side shader math with small per-frame uniform/push data.
3. Switch entity/model submission to indexed static mesh buffers + per-instance transforms (and GPU skinning path for MD5).
4. Introduce transient ring-buffer staging and avoid full-buffer memcpy for unchanged regions.
5. Add transparent draw sorting key path (material + depth buckets) to reduce overdraw artifacts while preserving batching.

## 4) Priority Implementation Backlog

P0 (visual correctness blockers):
1. Finish MD2/MD5 parity fix pass (pose/interpolation/skin correctness).
2. Resolve remaining particle family gaps (projectile trail variants, muzzle flashes, additive modes).
3. Fix outstanding sky artifact parity and validate all six faces/UV seams.
4. Fix bmodel initial-state stale world artifact at first render frame.

P1 (gameplay-visible fidelity):
1. Implement Vulkan equivalents for `gl_partstyle` and `gl_beamstyle`.
2. Add `RF_FLARE` behavior and related entity-flag parity paths.
3. Improve alpha-test/cutout precision and texture filtering parity for sprite/cutout assets.

P2 (performance and feature completeness):
1. CPU-footprint reduction plan (static GPU buffers + GPU dynamic shading).
2. Post-process parity track (fog/bloom/HDR baseline).
3. Native Vulkan shadowmapping stack aligned with WORR requirements.
