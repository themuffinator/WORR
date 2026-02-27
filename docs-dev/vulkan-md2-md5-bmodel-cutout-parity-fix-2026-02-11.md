# Vulkan renderer parity fixes (MD2/MD5, bmodel ghosting, alpha-cutout) - 2026-02-11

## Context
This pass addresses three Vulkan-native parity regressions against the OpenGL renderer:

1. MD2/MD5 model rendering instability and visibly incorrect model surfaces.
2. Inline bmodel movers leaving an apparent "initial state" ghost in world render.
3. Alpha-cutout surfaces rendering as scrambled blended geometry (for example muzzle-flash and cutout skin regions).

## Root causes

### 1) Alpha-cutout data was routed through blend passes
The Vulkan entity/world paths treated any texture with transparency (`VK_UI_IsImageTransparent`) as a blended surface. That routes cutout textures (binary/near-binary alpha) into the alpha pipeline with depth writes disabled, which causes self-overdraw/sort artifacts and visibly scrambled output on weapon/model cutout regions.

### 2) World-face filtering relied on a range that is not always valid
The Vulkan world mesh/lightmap build used a single `[firstface, firstface+numfaces)` range from model 0. On `q2dm1`, this range is invalid (`world_range_valid=0`), so the old code silently fell back to processing all BSP faces, including inline bmodel faces. That can leave static world geometry where moving brush entities should exclusively own those faces.

### 3) Per-vertex transform rebuilt entity basis repeatedly
The entity path rebuilt angle/scale basis in the hot per-vertex transform function. This increases CPU cost and multiplies numerical/consistency risk for large model batches.

## Implementation

### File: `src/rend_vk/vk_entity.c`

- Added cached per-entity transform representation:
  - `vk_entity_transform_t`
  - `VK_Entity_BuildTransform`
  - `VK_Entity_TransformPointWithTransform`
  - `VK_Entity_TransformPointInverseWithTransform`
- Switched MD2/MD5 and bmodel vertex transforms to use the cached transform once per entity draw path.
- Updated blend routing for model/sprite/bmodel passes:
  - MD2/MD5 now blend only when entity alpha is truly translucent (`RF_TRANSLUCENT` path via `VK_Entity_Alpha < 1`), not merely because texture has an alpha channel.
  - Sprite batching now uses color alpha for blend routing (cutout textures no longer force blend path by default).
  - Bmodel batching now uses explicit translucent surf flags/entity alpha and no longer forces blend just because texture is transparent.
- Added robust triangle validity handling:
  - MD5: skip triangles if any index is invalid (prevents undefined vertices being emitted).
  - MD2: added bounds guard for indices before vertex fetch/emit.

### File: `src/rend_vk/vk_world.c`

- Replaced world-face range heuristic with explicit per-face world mask:
  - `VK_World_BuildWorldFaceMask`
  - `VK_World_IsWorldFaceIndex`
  - `VK_World_ClearWorldFaceMask`
- New mask behavior:
  - Start from world model face range when valid.
  - Fallback to all faces when model 0 range is invalid.
  - Always subtract inline model face ranges (`models[1..]`) from world mask.
- Applied mask across world build/update paths:
  - Lightmap atlas build
  - Lightmap style dirty updates
  - Full lightmap atlas pixel rebuild
  - World mesh triangle batching
- Removed texture-transparency forced blend flagging in world mesh batching. Blend pass now tracks explicit translucent surf flags (`SURF_TRANS33/SURF_TRANS66`) instead of all alpha-channel textures.
- Added diagnostic log on map registration:
  - `VK_World_BuildWorldFaceMask: total=%d world=%d inline_cleared=%d world_range_valid=%d`

## CPU footprint impact

- Reduced repeated transform math in entity hot loops by precomputing transform basis/scale once per entity.
- Kept world-face ownership decisions precomputed at registration time, avoiding per-frame ownership reconstruction.
- Avoided additional GL fallback or cross-renderer redirection; all changes are native to `rend_vk`.

## Validation

### Build
- `meson compile -C builddir` succeeded.

### Runtime smoke
- `worr.exe +set r_renderer vulkan +set vk_md5_use 0 +map q2dm1` remained alive through smoke window.
- `worr.exe +set r_renderer vulkan +set vk_md5_use 1 +map q2dm1` remained alive through smoke window.
- Logs show world mask active on `q2dm1`:
  - `VK_World_BuildWorldFaceMask: total=4451 world=4397 inline_cleared=54 world_range_valid=0`

This confirms the previous invalid model-0 range condition exists on this map and that the new explicit mask path is engaged.

## Follow-up visual checks

1. Verify weapon model and muzzle flash cutout regions no longer appear blended/scrambled.
2. Verify moving brush entities no longer leave initial-state ghost geometry.
3. Compare MD2 and MD5 interpolation in motion (`vk_md5_use 0` vs `1`) on `q2dm1` and one model-heavy map.
