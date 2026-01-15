# Renderer Backend Map and Vulkan Plan

## OpenGL renderer (current state)

### Entry points and call flow
- `src/client/renderer.c`: loads external renderer DLLs when enabled, registers `r_renderer`, wires `renderer_import_t`, and owns the `renderer_export_t re` shim.
- `src/client/renderer.c`: `CL_InitRenderer` selects a `vid_driver` and calls `R_Init(true)`; `CL_ShutdownRenderer` calls `R_Shutdown(true)`.
- `src/client/precache.c`: `CL_PrepRefresh` -> `R_BeginRegistration`, followed by `R_EndRegistration` after precache completes.
- `src/client/screen.c`: per-frame flow: `R_BeginFrame` -> `R_RenderFrame` -> `R_EndFrame`.
- `src/rend_gl/main.c`: owns `R_Init`, `R_Shutdown`, `R_BeginRegistration`, `R_EndRegistration`, `R_BeginFrame`, `R_RenderFrame`, `R_EndFrame`.

### Render pass order (from `src/rend_gl/main.c`)
- Setup: copy `refdef_t`, update fog flags, rebuild lightmaps if dirty, bind postprocess FBOs.
- 3D: `GL_DrawWorld` -> draw bmodels -> opaque entities -> alpha back -> alpha faces -> beams -> particles -> occlude flares -> flare draw -> alpha front -> debug draw.
- 2D: unbind FBO, `GL_Setup2D`, postprocess (bloom/waterwarp), polyblend, debug lightmap, error checks.
- Present: swap buffers in `R_EndFrame`, optional fence sync.

### Module responsibilities
- `src/rend_gl/main.c`: central renderer state, init/shutdown, view/frustum setup, entity sorting, render pass orchestration, cvar registration.
- `src/rend_gl/qgl.c` / `src/rend_gl/qgl.h`: OpenGL function loading and capability probing.
- `src/rend_gl/state.c`: GL state cache, backend selection (`legacy` vs `shaders`), 2D/3D setup, state bits.
- `src/rend_gl/shader.c`: GLSL program management and shader-backed backend.
- `src/rend_gl/legacy.c`: fixed-function/ARB fragment program backend.
- `src/rend_gl/tess.c`: batching/tessellation, vertex array management.
- `src/rend_gl/surf.c`: world surface rendering (lightmaps, water/turbulence).
- `src/rend_gl/world.c`: world/BSP registration, visibility, lightstyles.
- `src/rend_gl/models.c`: model registration (alias, sprite, brush), bridges to mesh code.
- `src/rend_gl/mesh.c`: model mesh draw paths (including MD5 support).
- `src/rend_gl/images.c` + `src/rend_gl/images.h`: image registry, caching, texture source decoding.
- `src/rend_gl/texture.c`: texture uploads, lightmaps, renderbuffers/FBOs for postprocess.
- `src/rend_gl/draw.c`: 2D HUD/UI draw API (pics, chars, strings, raw).
- `src/rend_gl/sky.c`: skybox/cubemap handling and sky draw.
- `src/rend_gl/debug.c` / `src/rend_gl/debug_text.c`: debug primitives and debug text rendering.
- `src/rend_gl/hq2x.c`: HQ2x upscaler used by the image pipeline.
- `src/renderer/renderer_api.c`: `Renderer_GetAPI` export for external renderer builds.
- `src/rend_gl/gl.h`: internal GL renderer types and globals.
- `src/rend_gl/arbfp.h`: ARB fragment program data for legacy path.

### Video backends
- `src/windows/wgl.c`, `src/windows/egl.c`: Win32 OpenGL context creation and swap.
- `src/unix/video/x11.c`, `src/unix/video/wayland.c`, `src/unix/video/sdl.c`: Unix/SDL video backends.
- These drivers call `R_GetGLConfig` to pick GL context attributes and drive `vid->swap_buffers`.

### Renderer API surfaces
- `inc/renderer/renderer.h`: renderer entry points and shared types (`refdef_t`, `entity_t`, etc).
- `inc/renderer/renderer_api.h`: renderer import/export ABI for external renderers.

## Vulkan renderer target (planned)

### Directory layout
- `src/renderer/`: shared renderer code (image decoding, model parsing, common math/helpers).
- `src/rend_gl/`: OpenGL-specific implementation (moved from current `src/renderer`).
- `src/rend_vk/`: Vulkan implementation (new backend).

### Vulkan backend scope
- Device and swapchain setup with per-platform surface creation.
- Surface creation uses `vid_driver_t.get_native_window` to fetch platform handles.
- Feature parity with OpenGL: world surfaces, models, particles, 2D UI, lightmaps, fog, bloom, waterwarp, debug draw.
- Runtime selection via `r_renderer` with external renderers enabled.
 - Build toggle: Meson `vulkan` option controls whether the Vulkan renderer library is built.

### Vulkan backend status
- `src/rend_vk/vk_main.c` currently provides renderer API stubs to keep builds green.
- Platform handle export + build plumbing are in place; rendering implementation is still required.

### High-level execution plan
1. Reorganize GL sources into `src/rend_gl/` and isolate shared bits in `src/renderer/`.
2. Add Vulkan option/build targets and renderer selection plumbing.
3. Implement Vulkan backend modules (device, swapchain, pipelines, draw, world, models, images, debug).
4. Validate parity against GL (render order, cvars, visuals, performance) and build on all supported platforms.
