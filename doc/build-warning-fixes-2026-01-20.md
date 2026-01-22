# Build Warning Fixes (2026-01-20)

## Overview
This update focuses on eliminating compiler warnings emitted during the build,
without changing runtime behavior.

## Changes
- `src/rend_vk/vkpt/vertex_buffer.c`: Updated no-argument function definitions
  to use explicit `(void)` parameters for strict-prototypes compliance.
- `src/rend_vk/vkpt/textures.c`: Updated no-argument function definitions to
  use `(void)` parameters; marked `create_invalid_texture` and
  `destroy_invalid_texture` as `static` to avoid missing-prototypes warnings.
- `src/common/files.c`: Replaced `struct stat` with `Q_STATBUF` in
  `FS_LastModified` to match the platform-specific `os_stat` signature.
- `src/unix/video/sdl.c`: Cast `event->type` to `int` in `switch` statements to
  avoid `-Wswitch-enum` warnings while preserving behavior.
- `src/rend_vk/refresh/stb/stb_image_write.h`: Added forward declarations for
  `stbi_zlib_compress` and `stbi_write_png_to_mem` in the implementation block
  so `-Wmissing-prototypes` no longer triggers when building `stb.c`.
- `src/rend_vk/refresh/images.c`: Marked `stbi_write` as `static`, added a local
  prototype for `load_img`, and removed the redundant function-pointer check
  in `is_render_hdr()` to avoid `-Wmissing-prototypes` and
  `-Wpointer-bool-conversion` warnings.

## Notes
- All adjustments are signature/prototype or type-alignment fixes; functional
  logic is unchanged.
