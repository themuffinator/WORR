# Vulkan Native UI Bootstrap (2026-02-10)

## Summary
Native `vulkan` renderer previously had no draw implementation for 2D APIs (`R_DrawPic`, text, fills, raw image updates), so menu/console appeared blank even though the backend initialized.

This change implements a native Vulkan 2D draw path for the `vulkan` renderer and wires it into frame recording.

## Root Cause
`src/rend_vk/vk_main.c` implemented swapchain clear/present only; all 2D and image APIs were stubs.

## Implementation

### New Vulkan UI subsystem
Added `src/rend_vk/vk_ui.c` with `src/rend_vk/vk_ui.h` interface:
- Descriptor set layout + descriptor pool for sampled UI textures.
- Sampler creation.
- Graphics pipeline for 2D textured quads (`src/rend_vk/vk_ui2d_spv.h` shaders).
- Dynamic CPU draw queue (vertices/indices/draw commands).
- Host-visible vertex/index buffers uploaded each frame.
- Texture/image registration and updates:
  - `VK_UI_RegisterImage`
  - `VK_UI_RegisterRawImage`
  - `VK_UI_UpdateImageRGBA`
  - `VK_UI_UpdateRawPic`
- UI draw primitives:
  - pics (`DrawPic`, `DrawStretchPic`, sub-pic, rotate, keep-aspect)
  - fills (`DrawFill8`, `DrawFill32`)
  - raw blit (`DrawStretchRaw`)
  - tiled clear (`TileClear`)
- GL-style UI scale + clip behavior via shared `renderer/ui_scale.c`.

### vk_main integration
Updated `src/rend_vk/vk_main.c`:
- Initialize/shutdown native UI subsystem with Vulkan context lifetime.
- Recreate/destroy UI pipeline resources with swapchain lifecycle.
- Record UI draws inside swapchain render pass (`VK_UI_Record`).
- Replace old stub renderer API implementations with `VK_UI_*` delegates.
- Implement GL-style character/string and kfont drawing logic using Vulkan sub-pic draws.
- Compute kfont texture scale (`sw`, `sh`) from runtime image dimensions.

### Build wiring
Updated `meson.build` native Vulkan source group (`renderer_vk_src`) to include:
- `src/rend_vk/vk_ui.c`
- `src/rend_vk/vk_ui.h`
- `src/rend_vk/vk_ui2d_spv.h`
- `src/rend_vk/refresh/stb/stb.c`
- shared `src/renderer/ui_scale.c`

## Validation
- `meson compile -C builddir` succeeds.
- Native `vulkan` smoke run succeeds and initializes fonts/UI image assets.
- Native `vulkan` smoke run with `+map q2dm1` succeeds (no renderer crash/fatal during map load).

## Current Limits
- 3D world rendering in native `vulkan` is still not implemented in `R_RenderFrame`.
- This change restores native Vulkan 2D/UI rendering path (menu/console/text/pics), not full map geometry/lightmap rendering.
