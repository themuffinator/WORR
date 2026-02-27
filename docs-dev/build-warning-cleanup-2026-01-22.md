# Build and warning cleanup (2026-01-22)

## Goal
Resolve the SDL3_ttf/harfbuzz build configuration error, enable Harfbuzz+FreeType shaping in the fallback path, and remove build warnings introduced by unused helpers or lax prototypes.

## Build system changes
- Added SDL3_ttf/harfbuzz/freetype fallback dependencies and link them into client deps when enabled.
- Configured harfbuzz fallback options for a minimal build: C++14, `_CRT_SECURE_NO_WARNINGS`, freetype enabled, and optional libs/tools disabled.
- Added freetype fallback options to avoid optional libs (brotli/bzip2/libpng) and keep zlib internal.
- Updated `subprojects/SDL3_ttf-3.2.2/meson.build` to require `meson_version >= 0.56.0` to avoid `allow_fallback` warnings.
- Added `_CRT_SECURE_NO_WARNINGS`/`_CRT_NONSTDC_NO_WARNINGS` for MSVC/clang on Windows in `subprojects/harfbuzz-11.4.1/meson.build`.
- Updated `subprojects/jsoncpp-1.9.5/.meson-subproject-wrap-hash.txt` to match the current wrap file hash.

## Source warning cleanup
- `inc/refresh/debug.h`: include `shared/list.h` so `list_t` is defined.
- Vulkan renderer:
  - `src/rend_vk/vkpt/main.c`: marked internal helpers `static`, removed unused `R_AddDecal`, tightened `const` usage, and converted no-arg functions to `void` prototypes.
  - `src/rend_vk/vkpt/matrix.c` + `src/rend_vk/vkpt/vkpt.h`: make `create_view_matrix` accept `const refdef_t *`.
  - `src/rend_vk/vkpt/transparency.c`: `cast_u32_to_f32_color` is now `static` and no-arg functions use `void` prototypes.
  - `src/rend_vk/vkpt/vertex_buffer.c`: removed unused `total_prims`, made primbuf helpers `static`, and added `void` prototypes.
  - `src/rend_vk/vkpt/bsp_mesh.c`: removed unused `dot2` and made AABB/tangent helpers `static`.
  - `src/rend_vk/vkpt/precomputed_sky.c`: guarded `_USE_MATH_DEFINES`, made internal helpers `static`, and used `void` prototypes for no-arg functions.
  - `src/rend_vk/vkpt/fsr.c`: added clang/GCC diagnostic push/pop around FSR headers and used `void` prototypes for no-arg functions.
  - `src/rend_vk/third_party/tinyobj_loader_c.h`: moved the `TINYOBJ_UNUSED` attribute before `static` on `dynamic_fgets` to silence clang `-Wgcc-compat`.
- Game/UI:
  - `src/game/sgame/menu/menu_page_welcome.cpp`: mark unused helpers with `[[maybe_unused]]`.
  - `src/game/cgame/cg_draw.cpp`: removed an unused local variable in HUD string drawing.

## Build status
- `meson compile -C builddir` completes with compiler warnings resolved.
- A ninja warning about a truncated `.ninja_deps`/`.ninja_log` may still appear; deleting those two files in `builddir` clears it if needed.
