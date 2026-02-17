# Vulkan UI draw-capacity crash fix (2026-02-10)

## Context
- Renderer: `vulkan` (`worr_vulkan_x86_64.dll`)
- Symptom: immediate access violation during UI submission (`VK_UI_Record`) on startup / early map load.
- Observed call site: `vkCmdDrawIndexed` in `src/rend_vk/vk_ui.c`.

## Root cause
UI host buffer growth was keyed off `draw_count` instead of actual geometry counts.

In `VK_UI_EnqueueQuad` and `VK_UI_EnqueueRotatedQuad`, capacity checks used:
- `needed_quad_count = vk_ui.draw_count + 1`

This is incorrect because UI quads are aggressively merged into existing draw calls when descriptor set and scissor match. During merged submission:
- `draw_count` can stay flat for many quads.
- `vertex_count` and `index_count` keep increasing.
- Host arrays (`vk_ui.vertices`, `vk_ui.indices`) can overflow before reallocation.

That host-side overflow can corrupt renderer memory and later surface as a driver crash when recording or executing `vkCmdDrawIndexed`.

## Implementation
File changed:
- `src/rend_vk/vk_ui.c`

### 1) Decoupled capacity dimensions
`VK_UI_EnsureDrawCapacity` now receives explicit required counts:
- `needed_vertices`
- `needed_indices`
- `needed_draws`

This removes the false 1:1 coupling between draw-call count and geometry count.

### 2) Correct enqueue-time growth checks
Both enqueue paths now request capacity using current geometry totals:
- vertices: `vk_ui.vertex_count + 4`
- indices: `vk_ui.index_count + 6`
- draws: `vk_ui.draw_count + 1`

This guarantees host-side arrays grow whenever quad geometry grows, even if draw calls are merged.

### 3) Defensive draw-range validation
Before issuing `vkCmdDrawIndexed`, UI recording now validates:
- `draw->first_index < vk_ui.index_count`
- `draw->index_count <= vk_ui.index_count - draw->first_index`

Invalid ranges are skipped with a warning log instead of submitting undefined data to Vulkan.

## Validation performed
- Rebuilt Vulkan renderer DLL with Meson/Ninja.
- Ran:
  - `worr.exe +set r_renderer vulkan +set developer 1 +set logfile 1 +set logfile_name vk_ui_crash_test +set deathmatch 1 +set cheats 1 +map q2dm1 +quit`
- Result:
  - Vulkan initialized successfully.
  - Map `q2dm1` loaded.
  - No startup fatal error / no immediate `VK_UI_Record` crash.

## Notes
- This fix is Vulkan-native and does not route to OpenGL.
- The safeguard in `VK_UI_Record` is intentionally conservative to prevent driver-side crashes from bad draw ranges in future regressions.
