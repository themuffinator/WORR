# RTX Renderer Directory Move (`src/rend_rtx`) - 2026-02-10

## Summary

The RTX renderer implementation has been moved out of `src/rend_vk` into a dedicated `src/rend_rtx` tree.
This removes source-level ambiguity between:

- Native Vulkan legacy renderer (`src/rend_vk`)
- RTX renderer (`src/rend_rtx`)
- OpenGL renderer (`src/rend_gl`)

This aligns the codebase with the project renderer split policy and makes future parity/debug work safer.

## What was moved

The following RTX source groups were relocated with `git mv`:

- `src/rend_vk/vkpt` -> `src/rend_rtx/vkpt`
- `src/rend_vk/third_party` -> `src/rend_rtx/third_party`
- RTX refresh glue files:
  - `src/rend_vk/refresh/debug.c` -> `src/rend_rtx/refresh/debug.c`
  - `src/rend_vk/refresh/debug_text.c` -> `src/rend_rtx/refresh/debug_text.c`
  - `src/rend_vk/refresh/images.c` -> `src/rend_rtx/refresh/images.c`
  - `src/rend_vk/refresh/model_iqm.c` -> `src/rend_rtx/refresh/model_iqm.c`
  - `src/rend_vk/refresh/models.c` -> `src/rend_rtx/refresh/models.c`
  - `src/rend_vk/refresh/refresh_ptrs.c` -> `src/rend_rtx/refresh/refresh_ptrs.c`
  - `src/rend_vk/refresh/debug_fonts/*` -> `src/rend_rtx/refresh/debug_fonts/*`

## Build system refactor

`meson.build` was updated so RTX targets now consume sources from `src/rend_rtx`:

- `renderer_vk_rtx_src` paths now reference `src/rend_rtx/vkpt/*`
- RTX refresh glue paths now reference `src/rend_rtx/refresh/*`
- RTX shader input lists now reference `src/rend_rtx/vkpt/shader/*`
- RTX include path now references `src/rend_rtx/third_party`

Additional cleanup:

- Native Vulkan target include directories were cleaned to remove unnecessary coupling to `src/rend_rtx/third_party`.

## Shared code retained intentionally

`src/rend_vk/refresh/stb/stb.c` was intentionally left in place and remains shared by Vulkan variants.
This avoids needless duplication of stb support code while preserving renderer separation at the feature/module level.

## Validation

Both renderer DLLs were rebuilt successfully after the move:

- `worr_rtx_x86_64.dll`
- `worr_vulkan_x86_64.dll`

No `src/rend_vk/vkpt` or `src/rend_vk/third_party` source references remain in active source/build wiring.

## Notes

- Existing documentation predating this move may still mention historical `src/rend_vk/vkpt/...` paths.
- Functional behavior changes were not introduced by this refactor; this is a structural and build-wiring separation pass.
