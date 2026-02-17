# Vulkan q2dm1 startup/map-load fixes (2026-02-10)

## Scope
This document captures the Vulkan external renderer startup and `q2dm1` map-load stabilization work performed on 2026-02-10.

Goal for this pass:
- Vulkan renderer must at least reach menu + console reliably.
- Vulkan `+map q2dm1` must progress through client precache and into live map state without fatal allocator assertions.

## Symptoms observed

### 1) Immediate startup crash during shader loading
- Process exited with fail-fast (`0xC0000409`) while loading Vulkan shaders.
- Call path pointed into `vkpt_load_shader_from_basedir` in `src/rend_vk/vkpt/main.c`.

### 2) Early Vulkan image creation failures
- Vulkan validation reported repeated `vkCreateImage` failures due to zero width/height.
- This happened before the renderer had a valid active video mode extent.

### 3) Menu/console assertion in draw path
- Runtime assert:
  - file: `src/rend_vk/vkpt/draw.c`
  - line: 124
  - expression: `tex_handle`
- Triggered by missing UI art paths that returned handle `0`.

### 4) q2dm1 load fatal allocator assertion
- Fatal popup:
  - `recursive error after: Z_Free: assertion '(z)->magic == Z_MAGIC && (z)->tag != TAG_FREE' failed`
- cdb stack (first fatal site) showed:
  - `worr!Z_Free`
  - `worr_vulkan_x86_64!IMG_Unload_RTX`
  - `worr_vulkan_x86_64!R_SetSky`
  - `worr!CL_SetSky`
  - `worr!CL_PrepRenderer`
- This identified an invalid free in renderer image unload.

## Root causes and fixes

### A) Cross-CRT `FILE*` misuse in Vulkan DLL
- File: `src/rend_vk/vkpt/main.c`
- Function: `vkpt_load_shader_from_basedir`
- Problem:
  - Shader file opened through engine import (`Q_fopen`) but consumed by renderer CRT functions (`fseek/fread/fclose`) in the external renderer module.
  - Cross-module CRT ownership mismatch caused startup instability.
- Fix:
  - Open shader files with renderer-local `fopen` in this path.
  - Keep file operations in one CRT domain.

### B) Vulkan init order used zero-size extent
- File: `src/rend_vk/vkpt/main.c`
- Function: `R_Init(bool total)`
- Problem:
  - Vulkan image/swapchain setup could execute before a committed video mode, producing `0x0` extents.
- Fix:
  - Call `vid->set_mode()` immediately after `vid->init()` (guarded) before texture/swapchain dependent setup.

### C) Draw path assumed menu images always resolve
- File: `src/rend_vk/vkpt/draw.c`
- Problem:
  - Hard asserts on missing texture handles (`0`) in menu/pic draw helpers.
- Fix:
  - Hardened image-handle use:
    - fallback to `TEXNUM_WHITE` for invalid handles where appropriate.
    - avoid calling `IMG_ForHandle(0)`.
    - safely skip draw in invalid cases instead of fatal assert.
  - Included safe path for kfont texture scaling initialization.

### D) STB decode buffer freed with wrong allocator
- File: `src/rend_vk/refresh/images.c`
- Function: `IMG_LoadSTB`
- Problem:
  - STB decoders returned `malloc`-backed buffers.
  - VKPT image unload path frees `image->pix_data` with `Z_Free`/renderer allocator semantics.
  - Result: invalid free during map registration/shutdown transitions (observed in `R_SetSky` path).
- Fix:
  - In STB loader, copy decoded data into renderer-owned memory (`IMG_AllocPixels`).
  - Immediately release STB temporary buffer via `stbi_image_free`.
  - Keeps `image->pix_data` allocator ownership consistent for VKPT unload/reload paths.

## VS Code debug launch support
- File: `.vscode/launch.json`
- Added launch configuration:
  - `WORR (client, Vulkan q2dm1)`
- Behavior:
  - forces Vulkan renderer
  - starts with `+map q2dm1`
  - enables log capture and developer diagnostics
  - uses `preLaunchTask: meson: build`

## Validation performed

### Build
- `meson compile -C builddir`
- Result: successful link for `worr_vulkan_x86_64.dll` after fixes.

### Runtime
- Launched:
  - `worr.exe +set r_renderer vulkan +map q2dm1` (with log options)
- Result in latest run session:
  - process remained alive for timed run window (manually terminated after timeout).
  - log reached:
    - map load (`loading q2dm1`)
    - renderer material registration (`BSP_LoadMaterials`)
    - begin/spawn transitions (`cs_primed -> cs_spawned`)
    - live menu/callvote/join UI commands while in map state
  - no `Z_Free` fatal/assertion in the latest session segment.

## Remaining known issues in logs (non-fatal in this pass)
- Missing optional assets/messages still present, e.g.:
  - `Couldn't read maps/sky/q2dm1.txt`
  - `Couldn't read maps/cameras/q2dm1.txt`
  - missing optional env precomputed sky textures
  - missing `gfx/menu_bg_*` assets (already handled safely in draw path)

These did not block menu/console rendering or `q2dm1` map entry after the fixes above.
