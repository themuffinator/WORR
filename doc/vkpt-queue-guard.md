vkpt queue guard and idle wrapper

Summary
- Added a queue resolver that re-fetches the graphics queue when a submission/idle call sees a null queue handle.
- Submission now validates command buffers and logs explicit vkEndCommandBuffer/vkQueueSubmit failures.
- Replaced direct vkQueueWaitIdle calls in vkpt modules with a safe wrapper to avoid driver access violations when queue handles are invalid.

Why
- A null queue handle during vkpt initialization (notably vkpt_create_images) can crash inside the Vulkan driver.
- The previous code path used _VK on vkEndCommandBuffer/vkQueueSubmit, which logs but continues, risking invalid submissions.
- The new guard provides a controlled failure path and recovers by re-acquiring the graphics queue when possible.

Details
- Added vkpt_resolve_queue() in src/rend_vk/vkpt/main.c to:
  - Early-out if the queue is already valid.
  - Re-fetch qvk.queue_graphics when missing but device/queue index are available.
  - Emit Com_SetLastError diagnostics on failure.
- vkpt_submit_command_buffer() now:
  - Validates the queue handle via vkpt_resolve_queue().
  - Rejects null command buffers.
  - Checks vkEndCommandBuffer and vkQueueSubmit results and reports errors.
- vkpt_queue_wait_idle() added and exposed via VKPT_QUEUE_WAIT_IDLE() macro.
- Direct vkQueueWaitIdle(qvk.queue_graphics) calls in vkpt modules replaced with VKPT_QUEUE_WAIT_IDLE(qvk.queue_graphics).

Files touched
- src/rend_vk/vkpt/main.c
- src/rend_vk/vkpt/vkpt.h
- src/rend_vk/vkpt/textures.c
- src/rend_vk/vkpt/precomputed_sky.c
- src/rend_vk/vkpt/physical_sky.c
- src/rend_vk/vkpt/vertex_buffer.c
