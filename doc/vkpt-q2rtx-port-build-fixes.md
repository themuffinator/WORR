# Q2RTX vkpt port build fixes (WORR-2)

This document records the build-focused changes made while porting the Q2RTX vkpt Vulkan renderer into WORR-2. The goal was to resolve compile and link errors against WORR-2's external renderer API while keeping changes isolated to `src/rend_vk` and related headers.

## Renderer API alignment
- `src/rend_vk/vkpt/freecam.c`: avoid macro clashes with `Key_IsDown` by undefining/redefining around `client/keys.h`.
- `src/rend_vk/refresh/debug_text.c`: added `R_AddDebugLine` forward decl and included `refresh/refresh.h` so renderer API macros resolve.
- `src/rend_vk/refresh/model_iqm.c`: added local `QuatCopy` and `AddPointToBoundsLocal` helpers to avoid missing shared helpers.
- `src/rend_vk/refresh/models.c`: added `LittleBlock` to match the GL helper used by model validation.
- `src/rend_vk/vkpt/models.c`: added `LittleBlock`; swapped `Hunk_Alloc` to `Hunk_TryAlloc` to match renderer import API; replaced `COM_FilePath` with `COM_SplitPath` and trimmed trailing separators; removed `CL_PrepRefresh` call from the material reload command (now prints guidance).
- `src/rend_vk/vkpt/material.c`: include order fixed to ensure `vkpt.h` overrides macros; swapped `Com_HashString` to `Com_HashStringLen` for import compatibility.
- `src/rend_vk/vkpt/main.c`: `scr_viewsize` made static; added `R_ClampScale` to satisfy renderer export; added `cvar_pt_particle_emissive` registration; introduced `vkpt_freep` helper and replaced `Z_Freep` usage in vkpt modules.
- `src/rend_vk/vkpt/buddy_allocator.c`: included `refresh/refresh.h` so `Z_*` and related import macros are mapped.
- `src/rend_vk/vkpt/fog.c`: replaced `Cmd_ParseOptions`/`cmd_optarg` usage with manual option parsing; fixed `softface` index scope.
- `src/rend_vk/refresh/images.c`: replaced `Cmd_ParseOptions`/`cmd_optarg` usage with manual option parsing for `-S/--save`.

## Ported helper headers
- `inc/format/iqm.h`: added the IQM format header (copied from Q2RTX) for `model_iqm.c`.
- `inc/refresh/models.h`: added `MOD_MALLOC_ALIGN` and `MOD_Malloc` macro using `Hunk_TryAlloc`, added md2/md3 includes, and updated `MOD_Validate*` prototypes to use typed model pointers.

## Vulkan vkpt compatibility fixes
- `src/rend_vk/vkpt/draw.c`: `R_ClearColor` now uses `COLOR_U32_WHITE` to match WORR color constants.
- `src/rend_vk/vkpt/textures.c`: added alloca includes; `lazy_image_create` now passes `images_local` only when device groups are enabled.
- `src/rend_vk/vkpt/bsp_mesh.c`: fixed surfedge indexing via `bsp_surfedge_vertex`; updated `get_surf_plane_equation` signature and call sites; added a `tinyobj` file reader callback and updated to the new `tinyobj_parse_obj` signature; replaced `Q_rand` with `Com_SlowRand`; replaced `Z_Realloc` usage with `Z_Malloc`/`Z_Free` and manual copy.
- `src/rend_vk/vkpt/shadow_map.c`: added `vkpt_frand` wrapper using `Com_SlowRand` and used it in `sample_disk`.
- `src/rend_vk/vkpt/path_tracer.c`: added alloca includes for portability.
- `src/rend_vk/vkpt/tone_mapping.c` and `src/rend_vk/vkpt/profiler.c`: `R_DrawString` calls now provide `COLOR_WHITE` explicitly.

## Third-party glue
- `src/rend_vk/refresh/stb/stb.c`: switched STB allocators to `malloc`/`realloc`/`free` and removed `zone` dependency (`shared/shared.h` + `<stdlib.h>` only).

## Build status
- `meson compile -C builddir` now completes for `worr_vulkan_x86_64.dll`. Remaining warnings are from missing prototypes in stb helpers and function-pointer boolean checks; no linker or compile errors remain.
