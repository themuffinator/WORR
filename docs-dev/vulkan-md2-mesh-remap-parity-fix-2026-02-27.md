# Vulkan MD2 Mesh Remap Parity Fix - 2026-02-27

Task IDs: `FR-01-T04`

## Summary
Aligned Vulkan MD2 mesh decoding with the RTX/GL alias-model approach to fix incorrect model mesh handling and stabilize MD2->MD5 toggle reload flows.

## Problem
The Vulkan MD2 loader built one unique vertex per raw triangle index and failed hard on any invalid triangle index pair. This diverged from RTX/GL handling and produced mesh topology/UV behavior differences on real content.

## Root Cause
`VK_Entity_LoadMD2` used a simplified path:
- `num_vertices = num_indices` (no xyz+st remap/dedup),
- direct per-corner vertex mapping,
- immediate load failure on first bad triangle.

RTX/GL loaders instead:
- skip only invalid triangles,
- remap triangle corners to unique vertices by `(index_xyz, st.s, st.t)`,
- emit compact final indices over the remapped vertex set.

## Implementation

File: `src/rend_vk/vk_entity.c`

Updated `VK_Entity_LoadMD2` to follow RTX-style mesh construction:
- Added MD2 header/size bounds validation parity checks:
  - triangle/vertex/frame/skin limits,
  - skin width/height limits,
  - frame-size lower/upper bounds.
- Added robust triangle ingestion:
  - invalid triangles are skipped instead of aborting the whole model,
  - valid corners are accumulated into temporary `vert_indices`/`tc_indices` arrays.
- Added remap/dedup pass:
  - deduplicate by identical xyz index and texture coordinates,
  - build compact `final_indices` and `num_vertices`.
- Rebuilt final MD2 buffers from remapped topology:
  - compact `indices`, compact `uv`, and per-frame `positions` indexed by remapped vertices.
- Preserved existing skin registration and runtime draw path usage.

## Validation
- Build: `meson compile -C builddir` succeeded.
- Staging: `python tools/refresh_install.py --build-dir builddir --install-dir .install --base-game baseq2 --platform-id windows-x86_64` succeeded.
- Repro command (MD2->MD5 toggle + map reload) exited cleanly:
  - `.install/worr.exe +set developer 1 +set logfile 2 +set r_renderer vulkan +set vk_md5_use 0 +map q2dm1 +set vk_md5_use 1 +map q2dm1 +quit`
