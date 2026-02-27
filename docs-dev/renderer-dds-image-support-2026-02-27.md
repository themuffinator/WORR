# Renderer DDS Image Support (2026-02-27)

## Summary
- Added first-class `.dds` texture loading support across OpenGL, Vulkan RTX, and Vulkan legacy UI image paths.
- Implemented a shared DDS decoder module to avoid backend-specific parsing drift.
- Extended texture override search order so `.dds` candidates are considered by default.

## Why
- WORR image pipelines supported `pcx/wal` plus common truecolor formats (`png/jpg/tga`) but did not decode DDS replacement assets.
- Modern texture packs frequently ship DDS content (including block-compressed BC formats), so DDS files were ignored or treated as invalid.

## Implementation

### 1) Shared DDS decoder module
- Added:
  - `inc/renderer/dds.h`
  - `src/renderer/dds.c`
- Public API:
  - `R_DecodeDDS(...)` decodes DDS file memory into RGBA8 output.
- Supported DDS input:
  - Legacy FOURCC compressed: `DXT1` (BC1), `DXT3` (BC2), `DXT5` (BC3)
  - DX10 header variants:
    - `R8G8B8A8_UNORM(_SRGB)`
    - `B8G8R8A8_UNORM(_SRGB)`
    - `B8G8R8X8_UNORM(_SRGB)`
    - `BC1/BC2/BC3` UNORM and SRGB variants
  - Legacy uncompressed RGB/RGBA/luminance/alpha encodings with bit-mask extraction
- Guardrails:
  - Rejects cubemaps, volume textures, texture arrays, and non-2D DX10 resources.
  - Enforces size bounds and header/data validation with explicit error messages.

### 2) OpenGL renderer integration
- Files:
  - `src/rend_gl/images.h`
  - `src/rend_gl/images.c`
- Changes:
  - Added `IM_DDS` image format enum.
  - Added `IMG_LoadDDS` loader that calls `R_DecodeDDS`.
  - Registered `"dds"` in `img_loaders`.
  - Extended texture-format search/override gating logic to include DDS.
  - Split init-time blocks so texture override cvars initialize independently of screenshot-format availability.

### 3) Vulkan RTX refresh integration
- Files:
  - `inc/refresh/images.h`
  - `src/rend_rtx/refresh/images.c`
- Changes:
  - Added `IM_DDS` image format enum.
  - Added `IMG_LoadDDS` and registered `"dds"` in `img_loaders`.
  - Set decoded DDS textures to `PF_R8G8B8A8_UNORM`.
  - Upgraded `r_texture_formats` parsing to support both token form (`"png jpg tga dds"`) and legacy compact form (`"pjtd"`).
  - Switched default `r_texture_formats` source to `R_TEXTURE_FORMATS`.

### 4) Vulkan legacy renderer UI integration
- File:
  - `src/rend_vk/vk_ui.c`
- Changes:
  - Added DDS decode branch in `VK_UI_LoadRgbaFromFile(...)`.
  - Extended extension fallback probe order to include `.dds`.
  - This enables DDS-based replacements for UI/world texture lookups that route through `VK_UI_RegisterImage`.

### 5) Build/default format updates
- File:
  - `meson.build`
- Changes:
  - Added `src/renderer/dds.c` to:
    - `renderer_src`
    - `renderer_vk_rtx_src`
    - `renderer_vk_src`
  - Added `dds` to `texture_formats`, which flows into `R_TEXTURE_FORMATS`.

## User-facing behavior
- `.dds` can now be used as a texture replacement candidate in the same extension-stripping workflow as PNG/JPG/TGA.
- Default texture replacement search order is now:
  - `"png jpg tga dds"`

## Notes / current scope
- Decoder output is RGBA8 (DDS content is expanded on CPU before upload in these paths).
- BC4/BC5/BC6H/BC7 and cubemap/volume/array DDS resources are intentionally rejected in this iteration.
