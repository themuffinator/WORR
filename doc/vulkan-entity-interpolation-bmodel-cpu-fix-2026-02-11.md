# Vulkan Entity Interpolation + BModel Movement + CPU Footprint Pass (2026-02-11)

## Scope
This pass addresses three user-visible Vulkan issues:
1. Model rendering remains incorrect.
2. No model animation interpolation.
3. BModel entity movement still does not update correctly.

Additionally, this pass reduces per-frame CPU work in the Vulkan world/entity paths.

## Root causes

### 1) Animation interpolation behavior diverged from OpenGL
File: `src/rend_vk/vk_entity.c`

Vulkan MD2/MD5 interpolation only enabled when RF lerp flags were present:
- MD2 and MD5 both gated `backlerp` behind `RF_FRAMELERP | RF_OLD_FRAME_LERP`.

OpenGL alias path uses `ent->backlerp` directly and always resolves frame interpolation.
This caused visibly missing animation lerp in Vulkan.

### 2) Frame resolution behavior diverged from OpenGL extended/non-extended handling
File: `src/rend_vk/vk_entity.c`

Vulkan used modulo frame selection unconditionally (`frame % num_frames`).
OpenGL distinguishes:
- `fd->extended`: modulo frames.
- non-extended: out-of-range frames clamp/fallback to 0.

This mismatch could pick unexpected frames and produce incorrect model poses.

### 3) BModel texture and lighting path was too expensive and unstable
File: `src/rend_vk/vk_entity.c`

Each frame, each inline BSP face could:
- Re-register texture lookup path and size query.
- Run per-vertex `VK_World_LightPoint` calls.

This increased CPU cost and made bmodel path sensitive under movement-heavy scenes.

### 4) Static world-face filtering relied on pointer comparisons
File: `src/rend_vk/vk_world.c`

World-only filtering in multiple loops used pointer range checks against `models[0].firstface`.
This worked in common cases, but index-range filtering is stricter and avoids edge conditions.

### 5) Vulkan world vertex update performed unnecessary full CPU loops
File: `src/rend_vk/vk_world.c`

`VK_World_UpdateVertexLighting` updated all world vertices every frame even when:
- no dynamic lights were present,
- no warp animation was active,
- no prior dynamic state needed restore.

This inflated CPU footprint in steady-state gameplay.

## Implementation

## A) OpenGL-consistent animation frame resolution and interpolation
File: `src/rend_vk/vk_entity.c`

Added helper:
- `VK_Entity_ResolveAnimationFrames(...)`

Behavior:
- Uses `fd->extended` semantics consistent with GL:
  - extended: modulo frame wrap.
  - non-extended: clamp invalid frames to 0.
- Uses `ent->backlerp` directly (clamped 0..1), always.
- Optimized no-lerp case: `oldframe = frame` when `backlerp == 0`.

Applied to:
- `VK_Entity_AddMD2(...)`
- `VK_Entity_AddMD5(...)`

Result:
- Restores missing model interpolation in Vulkan and reduces bad-frame pose selection.

## B) Native bmodel path CPU cleanup and stability
File: `src/rend_vk/vk_entity.c`

Added BSP texture cache state to `vk_entity_state_t`:
- `bmodel_texture_bsp`
- `bmodel_texture_handles`
- `bmodel_texture_sets`
- `bmodel_texture_inv_sizes`
- `bmodel_texture_count`

Added helpers:
- `VK_Entity_ClearBspTextureCache()`
- `VK_Entity_EnsureBspTextureCache(...)`
- Refactored `VK_Entity_GetBspFaceTexture(...)` to use cache (descriptor + inv size cached per texinfo).

Behavior changes:
- Texture descriptor/size resolution for bmodels is now cached per map BSP texinfo index.
- Texture transparency state is cached per texinfo and still drives alpha-pass routing.
- Per-frame bmodel rendering no longer performs repeated expensive image registration/size lookup churn.
- Bmodel vertex lighting changed from per-vertex lightpoint sampling to per-entity lit color in this path.
  - This significantly lowers CPU cost while preserving dynamic movement rendering.

Lifecycle:
- Cache is invalidated on BSP pointer change in `VK_Entity_RenderFrame`.
- Cache is freed in `VK_Entity_Shutdown`.

## C) Harden static world-only face filtering via face index ranges
File: `src/rend_vk/vk_world.c`

Refactored:
- `VK_World_GetFaceRange(...)` now returns `(first_index, count)` not face pointers.

Applied world-only filtering by integer index bounds in:
- lightmap atlas rebuild/update loops,
- lightmap packing loops,
- world mesh build loops.

Result:
- Stronger guarantee that static world geometry excludes submodel faces.
- Prevents stale static copies from conflicting with dynamic bmodel movement.

## D) Reduced world per-frame CPU updates
File: `src/rend_vk/vk_world.c`

Added runtime state:
- `has_warp_vertices`
- `vertex_dynamic_dirty`

Added helper:
- `VK_World_HasWarpVertices(...)` (computed during map registration)

Optimized `VK_World_UpdateVertexLighting()`:
- Early-out when no warp animation, no dynamic lights, and no dirty restore is needed.
- Only recomputes colors when dynamic lights are active, or when restoring from prior dynamic state.
- Tracks dirty state (`vertex_dynamic_dirty`) to avoid redundant full updates in steady state.

Result:
- Lower steady-state CPU footprint for Vulkan world rendering.

## Validation performed

1. Build validation
- Command: `meson compile -C builddir`
- Result: success (`worr_vulkan_x86_64.dll` built and linked cleanly).

2. Runtime smoke stability
- Vulkan launch with `+map q2dm1` remained alive for 10 seconds in this environment before forced stop.
- This confirms no immediate initialization/frame-loop crash from the changes.

## Notes
- All fixes are native Vulkan (`rend_vk`) and do not redirect to OpenGL paths.
- This pass prioritizes parity and CPU cost reduction while keeping architecture compatible with ongoing renderer split work.
