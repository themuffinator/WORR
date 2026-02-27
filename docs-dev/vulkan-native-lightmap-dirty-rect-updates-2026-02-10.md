# Vulkan Native Lightmap Dirty-Rect Updates (2026-02-10)

## Objective
Continue native Vulkan world-lighting work by replacing full-atlas lightmap uploads with dirty-rect updates when lightstyles change at runtime.

## Root Cause
- Runtime lightstyle refresh in `src/rend_vk/vk_world.c` rebuilt the lightmap atlas and uploaded the entire texture every time styles changed.
- This was correct functionally but inefficient versus legacy-style dirty updates.
- `src/rend_vk/vk_ui.c` only exposed full-image RGBA updates, so world code had no sub-rect upload path.

## Implementation

### 1) Added Vulkan UI sub-rect image upload API
- Files:
  - `src/rend_vk/vk_ui.h`
  - `src/rend_vk/vk_ui.c`
- Added:
  - `VK_UI_UpdateImageRGBASubRect(qhandle_t handle, int x, int y, int width, int height, const byte *pic)`
- Implemented sub-rect transfer path with:
  - transfer barrier to `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`
  - `vkCmdCopyBufferToImage` using non-zero `imageOffset`
  - barrier back to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`
- Preserves existing full-image API (`VK_UI_UpdateImageRGBA`) unchanged.

### 2) Switched world lightstyle refresh to dirty-region uploads
- File: `src/rend_vk/vk_world.c`
- Reworked style refresh path:
  - detect exactly which lightstyles changed (`VK_World_CollectChangedStyles`)
  - rebuild only faces that reference changed styles (`VK_World_FaceUsesChangedStyle`)
  - union touched face atlas blocks (including 1-texel padded borders)
  - upload only that dirty rectangle (`VK_World_UploadLightmapDirtyRect` -> `VK_UI_UpdateImageRGBASubRect`)
- Existing CPU atlas buffer remains authoritative and is updated in place.

### 3) Runtime diagnostics
- File: `src/rend_vk/vk_world.c`
- Added `Com_DPrintf` trace for lightstyle refresh events with:
  - changed style count
  - updated face count
  - uploaded dirty rectangle size and origin

## Validation
- Build:
  - `meson compile -C builddir` (success)
- Runtime smoke:
  - Vulkan: `worr.exe +set vid_renderer vulkan +set r_renderer vulkan +set developer 1 +map q2dm1 +wait 240 +quit` (exit code 0)
  - OpenGL regression smoke: `worr.exe +set vid_renderer opengl +set r_renderer opengl +map q2dm1 +wait 120 +quit` (exit code 0)

## Notes
- All work is native Vulkan (`rend_vk`) and does not redirect to OpenGL.
- This update reduces upload bandwidth during animated lightstyle changes while preserving behavior.
