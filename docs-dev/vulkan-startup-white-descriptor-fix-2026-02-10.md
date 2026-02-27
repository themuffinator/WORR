# Vulkan Startup Fix: `white descriptor unavailable` - 2026-02-10

## Symptom

Native Vulkan renderer failed immediately during startup with:

- `Couldn't initialize renderer: Vulkan entity: white descriptor unavailable`

## Root Cause

`VK_Entity_Init` registers a raw fallback texture (`**vk_entity_white**`) through `VK_UI_RegisterRawImage` before swapchain creation.

That raw-image path calls `VK_UI_SetImagePixels` -> `VK_UI_UploadImageData` -> `VK_UI_BeginImmediate`, which requires a valid `vk_context_t.command_pool`.

At the time of this call, `command_pool` was still null because it was only created inside `VK_CreateSwapchain`, which happens later in initialization.

Result:

- UI image descriptor allocation/upload path failed.
- Entity fallback white image handle could not resolve to a descriptor set.
- Renderer init aborted with `white descriptor unavailable`.

## Implementation

File changed: `src/rend_vk/vk_main.c`

1. Added `VK_CreateCommandPool(vk_context_t *ctx)` to create the Vulkan command pool as part of base context setup (after device creation, before module init).
2. Called `VK_CreateCommandPool` in `R_Init` immediately after `VK_CreateDevice`.
3. Removed command-pool creation from `VK_CreateSwapchain`.
4. Updated `VK_DestroySwapchain`:
   - It now frees swapchain command buffers via `vkFreeCommandBuffers` (if allocated).
   - It no longer destroys the command pool.
5. Updated `VK_DestroyContext`:
   - Command pool is now destroyed at context teardown (after renderer subsystem shutdown, before device destruction).
6. Added explicit guard in swapchain creation that errors if command pool is unavailable.

## Why this fixes it

UI image upload paths now have a valid command pool during early renderer initialization.
This allows default/fallback image descriptors to be created before swapchain resources, which matches how Vulkan UI/entity startup currently wires texture registration.

## Validation

Built successfully:

- `worr_vulkan_x86_64.dll`
- `worr_rtx_x86_64.dll`

Runtime smoke test passed:

- `builddir/worr.exe +set r_renderer vulkan +map q2dm1 +quit`
- Vulkan initialized, map registration completed, and no startup fatal error occurred.
