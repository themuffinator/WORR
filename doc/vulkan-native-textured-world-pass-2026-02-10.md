# Vulkan Native Textured World Pass (2026-02-10)

## Objective
Move the native `vulkan` renderer from a color-only BSP pass to a textured BSP pass with lightmap/lightgrid modulation, while staying fully native (`rend_vk`) and not redirecting rendering paths to OpenGL.

## Root Cause
- The native Vulkan world path in `src/rend_vk/vk_world.c` rendered only vertex colors and had no texture sampling.
- `src/rend_vk/vk_ui.c` loaded images via `stbi` only, so core Quake II wall textures (`.wal`) and many classic assets (`.pcx`) were not decoded natively.
- The Vulkan palette table (`d_8to24table`) was grayscale fallback only, which breaks indexed texture fidelity.

## Implementation

### 1) Native indexed texture pipeline support (`.pcx` / `.wal`)
- File: `src/rend_vk/vk_ui.c`
- Added native decode path for Quake indexed assets:
  - `VK_UI_LoadPcxRgba(...)`
  - `VK_UI_LoadWalRgba(...)`
  - shared unpack path `VK_UI_Unpack8ToRgba(...)` with transparent-edge color bleed handling.
- `VK_UI_LoadRgbaFromFile(...)` now:
  - detects extension
  - loads `.pcx` and `.wal` natively first
  - falls back to `stbi` for non-indexed formats.
- Extended fallback probe order in `VK_UI_LoadImageData(...)` to include `.wal`.
- Fixed path normalization behavior in `VK_UI_NormalizeImagePath(...)`:
  - `IT_PIC` / `IT_FONT` keep legacy `pics/` behavior.
  - other types (`IT_WALL`, etc.) now keep their native paths instead of being incorrectly forced into `pics/`.

### 2) Vulkan palette fidelity
- File: `src/rend_vk/vk_main.c`
- Added `VK_LoadPaletteFromColormap(...)` to load `pics/colormap.pcx` palette and initialize `d_8to24table`.
- `VK_InitPalette()` now:
  - prefers palette from `colormap.pcx`
  - falls back to grayscale with warning only if palette load fails.

### 3) Descriptor sharing for world texturing
- Files:
  - `src/rend_vk/vk_ui.h`
  - `src/rend_vk/vk_ui.c`
- Added public descriptor accessors:
  - `VK_UI_GetDescriptorSetLayout()`
  - `VK_UI_GetDescriptorSetForImage(qhandle_t)`
- This allows `vk_world` to bind per-texture descriptor sets without duplicating texture system code.

### 4) Textured BSP world batching and draw submission
- File: `src/rend_vk/vk_world.c`
- Upgraded world vertex format from:
  - `pos + color`
  to:
  - `pos + uv + color`
- Added texture-aware mesh build:
  - per-face texture resolve via `textures/<texinfo>.wal`
  - UV generation from `texinfo->axis/offset`
  - existing lightmap/lightgrid sample stays as vertex color modulation.
- Added batch generation (`vk_world_batch_t`) grouped by descriptor set to minimize binds.
- Recording path now binds descriptor sets and draws per batch.
- Pipeline layout now includes the UI descriptor set layout, keeping all texture sampling native Vulkan.
- Init order adjusted in `src/rend_vk/vk_main.c`:
  - `VK_UI_Init(...)` runs before `VK_World_Init(...)` so world pipeline layout can import the descriptor set layout.

### 5) Shader update
- Files:
  - `.codex_temp/vk_world.vert`
  - `.codex_temp/vk_world.frag`
  - `src/rend_vk/vk_world_spv.h` (regenerated)
- World shaders now sample a bound texture and multiply by vertex lighting color.

## Validation
- Build:
  - `meson compile -C builddir`
- Runtime smoke:
  - `worr.exe +set vid_renderer vulkan +set r_renderer vulkan +set developer 1 +map q2dm1 ...`
- Observed logs:
  - `VK_World_BuildMesh: vertices=29805 batches=1230 lightmapped=29805`
  - `VK_World_BeginRegistration: map=q2dm1 vertices=29805 batches=1230`
  - `VK_World_Record: rendered map=q2dm1 vertices=29805 batches=1230`

## Notes
- All work remains inside the native Vulkan renderer path (`src/rend_vk`), with no OpenGL rendering fallback.
