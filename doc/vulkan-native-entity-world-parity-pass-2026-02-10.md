# Vulkan Native Entity + World Parity Pass (2026-02-10)

## Scope
This change advances the native `vulkan` renderer (`worr_vulkan_*`) toward practical GL parity for gameplay rendering without redirecting to OpenGL.

Implemented focus areas:
- Native Vulkan entity rendering path (sprites, beams, MD2 alias models).
- World pass split into opaque + alpha pipelines.
- Sky surface rendering in native world batching.
- World transparency handling for `SURF_TRANS33` / `SURF_TRANS66` and transparent textures.
- Native warp/turbulence UV animation for `SURF_WARP`.
- Fragment alpha discard in native shaders for cutout textures.

## Architectural Changes

### 1. New native Vulkan entity module
- Added `src/rend_vk/vk_entity.c`
- Added `src/rend_vk/vk_entity.h`
- Added `src/rend_vk/vk_entity_spv.h`

Responsibilities:
- Model registration for:
  - `.sp2` sprite models
  - `.md2` alias models
- Per-frame entity vertex generation:
  - `RF_BEAM` quad generation
  - Sprite billboarding
  - MD2 frame interpolation via `frame/oldframe/backlerp`
- Opaque/alpha Vulkan pipelines for entity draws.
- Per-frame dynamic vertex uploads and descriptor batching.

`vk_main` integration:
- `R_RegisterModel()` now routes to native Vulkan model registration.
- Entity renderer lifecycle is wired into init/shutdown/swapchain creation/destruction.
- Entity pass now records between world and UI command recording.

### 2. World pass parity expansion
Updated `src/rend_vk/vk_world.c`:
- Surface batching now tracks per-batch render flags.
- Added distinct world pipelines:
  - Opaque (`depth write on`, blending off)
  - Alpha (`depth write off`, alpha blending on)
- `SURF_SKY` faces are now included in draw batching (instead of being skipped).
- Per-vertex metadata supports:
  - base UV
  - base alpha
  - warp/fullbright flags
- Per-frame vertex update now:
  - applies dynamic light color while preserving surface alpha
  - animates warp UVs when `vk_shaders 1`
  - keeps sky vertices fullbright
- Added cvars:
  - `vk_drawsky` (default `1`)
  - `vk_shaders` (default `1`)

### 3. Native shader updates
- Updated `.codex_temp/vk_world.frag` to discard near-zero alpha texels.
- Added `.codex_temp/vk_entity.vert` and `.codex_temp/vk_entity.frag`.
- Regenerated embedded SPIR-V headers:
  - `src/rend_vk/vk_world_spv.h`
  - `src/rend_vk/vk_entity_spv.h`

### 4. Shared renderer refactor for view/projection setup
To avoid duplicated renderer-side view push construction:
- Added `inc/renderer/view_setup.h`
- Added `src/renderer/view_setup.c`

Used by:
- native world path
- native entity path

This keeps renderer-shared logic in `renderer/` as requested.

### 5. UI API extension for render classification
Updated native UI module:
- `src/rend_vk/vk_ui.h`
- `src/rend_vk/vk_ui.c`

Added:
- `VK_UI_IsImageTransparent(qhandle_t pic)`

Used by world/entity batching to route transparent images to alpha pipeline.

## Build System Integration
Updated `meson.build`:
- Include new shared and Vulkan entity sources in renderer source groups.
- Include new Vulkan SPIR-V header and shared view setup header.

## Validation Performed
- Built native Vulkan DLL target:
  - `ninja -C builddir worr_vulkan_x86_64.dll`
- Built RTX DLL target (regression check after shared-source changes):
  - `ninja -C builddir worr_rtx_x86_64.dll`

Both targets linked successfully.

## Current Limitations / Follow-up
- Inline BSP entity models are still skipped in entity module and remain a follow-up for full moving-bmodel parity.
- Native sky rendering is now enabled at surface level; full GL sky feature parity (e.g. advanced skybox behavior) remains an incremental follow-up.
- This pass is intentionally focused on getting native Vulkan gameplay rendering paths active and stable for map loads such as `q2dm1`.
