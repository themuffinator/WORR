# Vulkan MD5 Mesh Parity Revision (Scale + Skin Routing) - 2026-02-27

Task IDs: `FR-01-T04`

## Summary
This is a follow-up Vulkan MD5 parity pass after initial frame/alpha fixes.  
The remaining regression path was in MD5-specific animation scale and skin routing behavior.

## Root Causes

### 1) `.md5scale` sidecar behavior missing in Vulkan
GL MD5 replacement loading supports optional `.md5scale` JSON sidecars that:
- set per-frame per-joint `scale` values
- set `scale_positions` flags per joint

Vulkan MD5 loading did not parse/apply `.md5scale`, so models depending on this metadata could produce incorrect skeletal deformation.

### 2) Skeleton builder overwrote joint scale every frame
`VK_MD5_BuildFrameSkeleton` unconditionally set `joint->scale = 1.0f`.  
Even if scale metadata were present, it would have been discarded.

### 3) MD5 draw path preferred MD2 skin handles
MD5 rendering selected MD2 skin handles first, only falling back to MD5 mesh shader skin.  
This can misroute textures for replacement MD5 meshes.

## Implementation

File: `src/rend_vk/vk_entity.c`

1. Added MD5 scale-sidecar support:
- New parser integration:
  - `VK_MD5_LoadScales`
- Uses `jsmn` (same stack as GL path) to parse `<anim>.md5scale`.
- Applies:
  - `joint_infos[i].scale_pos`
  - per-frame joint `scale` overrides in `md5->skeleton_frames`.

2. Captured joint names in hierarchy parse:
- Extended `vk_md5_joint_info_t` with:
  - `name[VK_MD5_MAX_JOINTNAME]`
  - `scale_pos`
- `VK_MD5_ParseAnim` now parses/stores joint names for sidecar matching.

3. Preserved scale during skeleton build:
- Removed unconditional `joint->scale = 1.0f` reset in `VK_MD5_BuildFrameSkeleton`.
- Applied GL-parity behavior:
  - if `scale_pos` is set, animated joint position is multiplied by `joint->scale`.

4. Adjusted MD5 skin routing:
- Added `VK_Entity_SelectMD5Skin`.
- MD5 path now uses:
  - entity custom skin overrides when explicitly present
  - otherwise MD5 mesh shader image (`mesh->shader_image`) as default.

5. Signature update:
- `VK_MD5_ParseAnim` now accepts animation path so it can derive/load `.md5scale`.

## Validation
- `meson compile -C builddir` succeeded.
- `python tools/refresh_install.py --build-dir builddir` succeeded.
- Vulkan smoke launch succeeded with MD5 replacements enabled:
  - `.install/worr.exe +set developer 1 +set r_renderer vulkan +set vk_md5_use 1 +set logfile 2 +map q2dm1 +quit`

## Expected Outcome
- MD5 models that rely on `.md5scale` metadata now deform/animate correctly in Vulkan.
- MD5 replacement meshes default to MD5-authored shader skins instead of MD2 skin handles.
