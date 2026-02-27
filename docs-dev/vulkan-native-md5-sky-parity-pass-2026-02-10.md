# Vulkan Native MD5 + Sky Parity Pass (2026-02-10)

## Summary
This change set improves native Vulkan renderer parity in two areas:
- MD5 replacement model loading/rendering for alias models.
- Skybox stability by decoupling sky rendering from world lightmap atlas content.

## Root Causes
- `rend_vk` had MD5 struct scaffolding but no active loader/parser/animation/render path, so Vulkan could not reliably match GL replacement-model behavior.
- Skybox rendering used the world lightmap descriptor for modulation. If atlas texel selection was invalid/non-white, sky output could be dark/incorrect.

## Implementation

### 1) Native MD5 path in `src/rend_vk/vk_entity.c`
- Added MD5 parsing helpers:
  - strict token parsing (`expect`, float/int/uint parsing, vector parsing)
  - mesh parser (`.md5mesh`)
  - animation parser (`.md5anim`)
  - frame skeleton build and quaternion/axis reconstruction
- Added MD5 lifetime management:
  - per-mesh buffer cleanup
  - skeleton frame cleanup
  - model cleanup integration in `VK_Entity_FreeModel`
- Added MD5 replacement loading for MD2 aliases:
  - tries `<model_dir>/md5/<model>.md5mesh` and `.md5anim`
  - attaches parsed replacement to the existing Vulkan model record
  - logs successful replacement loads with mesh/joint/frame counts
- Added runtime MD5 selection + draw path:
  - lerped skeleton support (`frame`/`oldframe` + `backlerp`)
  - CPU skinning from weights/joints into local vertex positions
  - world transform + texture coordinate emission
  - skin selection compatibility path (custom skin/MD2 skin fallback, then MD5 shader image)

### 2) MD5 control cvars (Vulkan)
- Added and wired Vulkan-native MD5 cvars:
  - `vk_md5_load` (default `1`) controls replacement loading at registration.
  - `vk_md5_use` (default `1`) controls runtime use of loaded replacements.
  - `vk_md5_distance` (default `2048`) LOD-style range gate for MD5 usage.

### 3) Skybox lightmap isolation in `src/rend_vk/vk_world.c`
- Added dedicated 1x1 white descriptor for sky lightmap modulation:
  - created at world init (`**vk_world_sky_lm_white**`)
  - bound on descriptor set 1 during skybox draw
  - world lightmap descriptor rebound after sky pass
- This removes sky dependence on current world atlas texel content.

### 4) Sky setup tracing
- Added debug traces for Vulkan sky setup path:
  - `R_SetSky` request trace in `src/rend_vk/vk_main.c`
  - successful `VK_World_SetSky` trace in `src/rend_vk/vk_world.c`

## Validation
- Build: `meson compile -C builddir` succeeded for `worr_vulkan_x86_64.dll`.
- Runtime smoke launches on Vulkan completed and logged:
  - map registration
  - large-scale MD5 replacement loads when enabled
- `vk_md5_load 0` smoke test: no MD5 replacement loads logged.

## Known Remaining Parity Gaps (vs GL)
- Full MD3 path parity is still incomplete in native Vulkan entity rendering.
- Player/view weapon model path (`RF_VIEWERMODEL`) is still intentionally skipped in native Vulkan entity draw.
- GL shader pipeline parity (`gl_shaders`-style advanced path) is not yet ported in full.
- Advanced material effects (full warp/transparency variant parity) still need incremental convergence.
- Sky visibility clipping to only visible sky polygons (GL-style skyface clipping) is not fully mirrored; current path draws a skybox pass when sky is enabled.

## Notes
- No OpenGL fallback/redirect was introduced; all changes are native Vulkan path updates.
