# Vulkan MD5 Mesh Parity Fix (Frame Resolve + Opaque Routing) - 2026-02-27

Task IDs: `FR-01-T04`

## Summary
This pass fixes a Vulkan-native MD5 model parity regression that caused visibly incorrect model mesh rendering (especially on replacement MD5 assets).

Two parity issues were corrected in `src/rend_vk/vk_entity.c`:
- MD5 animation frame resolve used MD5 frame count directly instead of GL-compatible base MD2 frame semantics.
- MD2/MD5 textured models were routed to alpha blending when skin textures had an alpha channel, which caused depth-write loss and self-overdraw artifacts on cutout/opaque content.

## Root Causes

### 1) MD5 frame resolve did not match GL replacement-model semantics
Vulkan resolved `ent->frame`/`ent->oldframe` against `md5->num_frames`.  
GL resolves frame validity against the base alias model frame domain first (MD2), then wraps into MD5 skeleton frames during skeleton sampling.  
When MD2 and MD5 frame counts diverge, Vulkan selected incorrect animation poses in non-extended/extended paths.

### 2) Texture-alpha forced MD2/MD5 into blend pipelines
Vulkan entity code treated `VK_UI_IsImageTransparent(skin)` as a reason to route MD2/MD5 through alpha blending.  
That disabled depth writes for many effectively opaque/cutout model skins, producing mesh-surface artifacts due to unsorted/self-overlapping triangles.

## Implementation

File: `src/rend_vk/vk_entity.c`

1. MD5 frame resolve alignment:
- In `VK_Entity_AddMD5`, frame resolve now uses:
  - `frame_count = model->md2.num_frames ? model->md2.num_frames : md5->num_frames`
- This preserves GL-style entity frame semantics while still using MD5 modulo behavior in skeleton sampling (`VK_Entity_LerpMD5Skeleton`).

2. MD2/MD5 opaque/alpha routing correction:
- Updated alpha classification in:
  - `VK_Entity_AddMD5`
  - `VK_Entity_AddMD2`
- New behavior: blend only when entity alpha is truly translucent:
  - `bool alpha = VK_Entity_Alpha(ent) < 1.0f;`
- Removed texture-alpha-based forcing for these model paths.

3. Diagnostics parity with GL:
- Added a warning in `VK_Entity_LoadMD5Replacement` when replacement animation has fewer frames than its MD2 base model:
  - `Com_WPrintf("%s has less frames than %s (%u < %u)\n", ...)`

## Validation
- Build:
  - `meson compile -C builddir` succeeded (recompiled `src/rend_vk/vk_entity.c`, linked `worr_vulkan_x86_64.dll`).
- Staging refresh:
  - `python tools/refresh_install.py --build-dir builddir` succeeded and refreshed `.install/`.
- Runtime smoke launch:
  - `.install/worr.exe +set r_renderer vulkan +set vk_md5_use 1 +map q2dm1 +quit` exited successfully.

## Expected Outcome
- MD5 replacement models use correct animation frame mapping relative to entity-provided MD2 frame indices.
- Opaque/cutout MD2/MD5 skins stay on opaque path with depth writes enabled, eliminating blend-pass mesh artifacts that appeared as incorrect model surfaces.
