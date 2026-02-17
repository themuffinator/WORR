# Vulkan model parity pass (animation/frame resolve + projection sync) - 2026-02-11

## Scope
This pass targets the remaining Vulkan model correctness reports (MD2/MD5 interpolation and first-person weapon presentation) while keeping rendering native in `rend_vk`.

## Root causes addressed

1. Frame resolve behavior in Vulkan had drifted from GL semantics.
- Vulkan was always modulo-wrapping frame indices.
- GL uses protocol-aware behavior:
  - extended protocol: modulo wrap
  - non-extended protocol: clamp invalid frame/oldframe to 0
- This can produce visibly incorrect interpolation pose selection in non-extended paths.

2. Entity projection was not guaranteed to match world projection clip planes.
- World pass computes dynamic `zfar` from BSP extents.
- Entity pass previously used fixed `zfar=8192`.
- Mismatched projection into the same depth buffer can cause incorrect model depth behavior.

3. Weapon-model depthhack projection parity was incomplete.
- GL uses a dedicated weapon frustum path (`cl_gunfov`, hand/gun reflection).
- Vulkan depthhack pass used only the world/view frustum for all depthhack geometry.

## Implementation details

### File: `src/rend_vk/vk_entity.c`

- Restored GL-compatible frame resolve semantics:
  - `VK_Entity_ResolveAnimationFrames` now uses `fd->extended` to choose wrap vs clamp behavior.
- Added clip-plane synchronized frame push builder:
  - `VK_Entity_BuildFramePush` now mirrors world clip-plane computation logic.
- Added GL-style weapon frustum builder:
  - `VK_Entity_BuildWeaponFramePush`
  - Applies `cl_gunfov` and hand/gun reflection handling.
- Extended entity batching metadata:
  - `vk_batch_t.weapon_model`
  - Depthhack batches now preserve whether geometry is weapon-model.
- Extended renderer frame state:
  - `frame_push_weapon`
  - `frame_weapon_active`
- Depthhack recording now swaps push constants per batch type:
  - weapon depthhack batches use weapon push constants
  - non-weapon depthhack batches use normal push constants

### Files: `inc/renderer/view_setup.h`, `src/renderer/view_setup.c`

- Added shared projection/view helper:
  - `R_BuildViewPushEx(fd, fov_x, fov_y, reflect_x, znear, zfar, out_push)`
- `R_BuildViewPush` now delegates to `R_BuildViewPushEx` with default reflect/fov values.
- This keeps projection setup reusable and centralized under shared renderer code.

## Validation

1. Build validation:
- `meson compile -C builddir` succeeded.
- Rebuilt `worr_vulkan_x86_64.dll` and dependent shared renderer objects.

2. Runtime smoke:
- `worr.exe +set r_renderer vulkan +set vk_md5_use 0 +map q2dm1 +quit`
- `worr.exe +set r_renderer vulkan +set vk_md5_use 1 +map q2dm1 +quit`
- Vulkan loads and map init completes in both modes without new asserts/crashes from this patch set.

## Notes

- One intermittent pre-existing runtime line was observed in smoke automation:
  - `Vulkan: frame submission failed: GetProcAddress(GetGameAPIEx) failed: The specified procedure could not be found.`
- This was not introduced by the model-path changes above (re-run behavior is inconsistent), but should be tracked separately if it remains reproducible in interactive runs.
