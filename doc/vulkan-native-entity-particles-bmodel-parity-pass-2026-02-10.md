# Vulkan Native Entity/Particle/BModel Parity Pass (2026-02-10)

## Goal
Address active Vulkan parity regressions reported during gameplay:
- Incorrect model rendering behavior.
- Missing particle rendering.
- Inline BSP model (`bmodel`) rendering not updating correctly.

## Root causes

1. Inline BSP entities were skipped in Vulkan entity submission.
- In `VK_Entity_RenderFrame`, any `ent->model & BIT(31)` path was dropped early.
- Result: dynamic/moving brush models were never drawn by Vulkan entity path.

2. Particle stream (`refdef_t::particles`) was never rendered by Vulkan.
- Vulkan had beam/sprite/md2 code paths but no particle primitive emission pass.
- Result: effects relying on classic particles were absent.

3. World mesh/lightmap build used all BSP faces, including submodels.
- Static world build included non-world faces, causing stale/incorrect bmodel behavior.
- Result: submodel faces could appear frozen/stale in static world mesh.

4. MD5 replacement default was enabled.
- `vk_md5_use` defaulted to `1`, making Vulkan default to replacement path where available.
- Result: baseline model behavior diverged from legacy expectations when MD5 content/pathing mismatched.

## Implementation

### 1) Native inline BSP model rendering in Vulkan entity pass
File: `src/rend_vk/vk_entity.c`

- Added new inline BSP draw path:
  - `VK_Entity_AddBspModel(const entity_t *ent, const bsp_t *bsp)`
- Added BSP surfedge vertex helper:
  - `VK_Entity_SurfEdgeVertexIndex(...)`
- Added face texture resolver:
  - `VK_Entity_GetBspFaceTexture(...)`

Behavior:
- For `ent->model & BIT(31)`, Vulkan now fetches world BSP and renders the referenced submodel natively.
- Triangulates each face fan from surfedges.
- Applies legacy-style face rejection using entity-local view/plane tests to avoid backface overdraw artifacts.
- Computes per-vertex transformed positions using entity origin/angles/scale.
- Uses BSP face texture coordinates (`texinfo axis/offset`) and binds per-face descriptor sets.
- Applies alpha for translucent surfaces (`SURF_TRANS33/SURF_TRANS66`) and texture transparency.
- Uses per-vertex lighting via `VK_World_LightPoint` for consistent lightgrid/lightmap-style lighting response.

### 2) Native particle rendering support
File: `src/rend_vk/vk_entity.c`

- Added Vulkan particle texture bootstrap:
  - `VK_Entity_InitParticleTexture()`
  - Generates a 16x16 radial alpha sprite, registered as raw image.
- Added frame particle emission pass:
  - `VK_Entity_AddParticles(const refdef_t *fd, const vec3_t view_axis[3])`

Behavior:
- Consumes `fd->particles` and emits legacy-style camera-facing particle triangles.
- Supports palette (`color != -1`) and explicit RGBA (`color == -1`) particles.
- Applies alpha and brightness scaling.
- Uses new Vulkan particle scale cvar:
  - `vk_partscale` (default `2`) to mirror legacy feel.

### 3) Model baseline behavior adjustment
File: `src/rend_vk/vk_entity.c`

- Changed MD5 replacement default:
  - `vk_md5_use` default from `1` -> `0`.

Behavior:
- Vulkan now defaults to classic MD2 path unless MD5 replacement is explicitly enabled.
- Reduces model correctness divergence during parity bring-up.

### 4) World-face restriction for static world mesh/lightmap build
Files:
- `src/rend_vk/vk_world.c`
- `src/rend_vk/vk_world.h`

- Added world model face-range selection helper:
  - `VK_World_GetFaceRange(...)`
- Restricted world-only loops in lightmap build/update and mesh build.
- Added BSP accessor for entity renderer:
  - `const bsp_t *VK_World_GetBsp(void)`.

Behavior:
- Static Vulkan world buffers now contain world faces only.
- Submodel faces are no longer baked into static world mesh, enabling correct dynamic bmodel updates via entity path.

## Validation

1. Build validation
- Command: `meson compile -C builddir`
- Result: success (`worr_vulkan_x86_64.dll` rebuilt cleanly).

2. Runtime smoke launch
- Attempted scripted Vulkan launch with `+map q2dm1` for short-run verification.
- Process launch worked but automated non-interactive run does not provide reliable in-session visual confirmation in this environment.

## Notes
- All Vulkan changes are native to `src/rend_vk` and do not redirect rendering flow to OpenGL.
- This pass is focused on immediate parity blockers (bmodels/particles/model default behavior) and keeps architecture compatible with ongoing Vulkan-native renderer split work.
