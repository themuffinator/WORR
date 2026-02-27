# Vulkan frame-freeze fence sync fix (2026-02-10)

## Symptom
- User-visible behavior: Vulkan renderer appears to freeze roughly a second after `q2dm1` loads.
- Runtime behavior before fix:
  - `vkQueueSubmit` could fail, and frame flow could stall.
  - Under instrumentation, world pass could trigger `VK_ERROR_DEVICE_LOST`, followed by repeated acquire failures.

## Root causes
1. **Fence lifecycle deadlock path in frame submit**
   - The frame fence was reset before command recording/submission.
   - If any failure happened after reset and before successful submit, the next frame could block indefinitely waiting on an unsignaled fence.

2. **CPU/GPU hazard on dynamic world vertex updates**
   - World vertex data is updated from CPU each frame (`VK_World_UpdateVertexLighting`) before the previous GPU frame was guaranteed complete.
   - This could race GPU reads of the same vertex buffer and destabilize the device.

## Changes made
### `src/rend_vk/vk_local.h`
- Added `vk_context_t::frame_submitted` to track whether a GPU submission is in flight.

### `src/rend_vk/vk_main.c`
- Added robust in-flight fence tracking:
  - Wait/reset/submit path now uses `frame_submitted` so failed submissions do not force a permanent fence wait on the next frame.
- Added `VK_WaitForSubmittedFrame(...)` helper and called it:
  - In `R_BeginFrame` before per-frame CPU-side renderer updates.
  - In `VK_DrawFrame` before reset/submit operations.
- Added explicit frame failure logging in `R_EndFrame`:
  - `Vulkan: frame submission failed: ...`
- Added temporary pass-isolation cvars used for debugging:
  - `vk_draw_world`
  - `vk_draw_entities`
  - `vk_draw_ui`

## Validation
- Rebuilt `worr_vulkan_x86_64.dll`.
- Ran Vulkan on `q2dm1` and monitored runtime:
  - Process remained responsive with ongoing CPU activity.
  - No repeated `VK_ERROR_DEVICE_LOST` / acquire-failure spam in post-fix probe logs.
- Isolation checks confirmed the problematic path was in the world pass before synchronization hardening.

## Notes
- This fix stays fully Vulkan-native and does not redirect to OpenGL.
- The pass-isolation cvars are useful for future Vulkan regression triage.
