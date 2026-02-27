# Vulkan Renderer Entity/World Parity and CPU Upload Pass (2026-02-11)

## Scope
This pass targets the native `rend_vk` renderer (non-RTX) for three active issues:

1. Incorrect model rendering paths (especially first-person and translucent-texture entities).
2. Missing/poor interpolation behavior in animated and moving entities.
3. High CPU overhead from per-frame Vulkan map/unmap uploads.

No OpenGL redirection was introduced.

## Root Causes Identified

### 1) `RF_VIEWERMODEL` entities were being drawn in the main VK entity pass
`RF_VIEWERMODEL` is used by the client for first-person shadow helper entities and should not be visibly drawn in the normal pass (GL skips them). Drawing them in Vulkan caused visible artifacts and incorrect first-person composition.

### 2) Entity alpha pipeline selection only considered `ent->alpha`
The Vulkan entity path selected alpha blending only when `RF_TRANSLUCENT`/`ent->alpha < 1`, but ignored per-texture transparency (sprite/skin alpha channels). This caused incorrect rendering for transparent model/sprite textures (including muzzle-flash-like assets) when entity alpha was 1.0.

### 3) Animation/frame resolve path could clamp to frame 0 for non-extended paths
Frame resolve behavior could degrade interpolation continuity by clamping out-of-range frame indices to 0 in non-extended cases.

### 4) CPU overhead from repeated `vkMapMemory`/`vkUnmapMemory`
Entity and world dynamic vertex uploads repeatedly mapped/unmapped host-visible buffers each frame/update. This increases CPU overhead and driver overhead.

## Implementation Details

### File: `src/rend_vk/vk_entity.c`

#### Visibility/composition parity
- Restored GL-like behavior by skipping `RF_VIEWERMODEL` entities in `VK_Entity_RenderFrame`.
- Kept depth-hack behavior focused on weapon/depthhack flags:
  - `RF_DEPTHHACK`
  - `RF_WEAPONMODEL`
- Removed `RF_VIEWERMODEL` from Vulkan depth-hack routing.

#### Texture transparency -> alpha pipeline selection
- `VK_Entity_AddSprite`: alpha pass now activates if texture is transparent (`VK_UI_IsImageTransparent`) even when entity alpha is 1.0.
- `VK_Entity_AddMD2`: same alpha routing fix based on selected skin transparency.
- `VK_Entity_AddMD5`: same alpha routing fix for selected mesh/override skin.
- `VK_Entity_AddBspModel`: transparency from BSP face texture cache now participates in pass routing (except `SURF_ALPHATEST` surfaces to preserve cutout-style behavior).

#### Interpolation behavior
- `VK_Entity_ResolveAnimationFrames`: frame and oldframe are now wrapped with modulo for valid interpolation continuity instead of clamp-to-zero fallback.
- `VK_Entity_BuildTransform`: transform origin now supports backlerp-based origin interpolation when `oldorigin` differs and `backlerp > 0`.

#### CPU upload optimization
- Added persistent mapped pointer for entity vertex buffer (`vertex_mapped`).
- Buffer memory is mapped once at allocation/resize and reused each frame.
- Removed per-frame map/unmap in `VK_Entity_RenderFrame`; upload now uses direct `memcpy` to persistent mapped memory.

### File: `src/rend_vk/vk_world.c`

#### CPU upload optimization (world path)
- Added persistent mapped pointer for world vertex buffer (`vertex_mapped`).
- Buffer memory is mapped once after allocation and reused.
- Removed repeated map/unmap from:
  - `VK_World_UploadVertices`
  - `VK_World_UpdateVertexLighting`
- Updates now write via direct `memcpy` into persistent mapped memory.

## Validation Performed

1. Compiled Vulkan renderer successfully:
   - `meson compile -C builddir`
   - Updated objects: `src/rend_vk/vk_entity.c`, `src/rend_vk/vk_world.c`.
2. Runtime smoke test (native Vulkan):
   - `worr.exe +set r_renderer vulkan +set developer 2 +map q2dm1 +quit`
   - Confirmed Vulkan init and map registration completed.
   - No new renderer init/assert/fatal failures observed.

## Notes

- Vulkan image decoding in this renderer remains `stb_image`-based (not libpng-linked decode in this path).
- This pass is focused on renderer correctness and CPU-side upload overhead; it does not add OpenGL fallbacks.

## Suggested Follow-up QA

1. In-map visual verification for first-person weapon composition and muzzle flashes.
2. Animation interpolation checks for MD2 and MD5 assets during motion/combat.
3. Moving brush model checks (doors/platforms) for stale-origin artifacts and continuous updates.
4. CPU frame-time sampling before/after in scenes with heavy dynamic world updates.
