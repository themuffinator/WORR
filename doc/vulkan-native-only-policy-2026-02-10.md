# Vulkan Native-Only Renderer Policy + Redirect Rollback (2026-02-10)

## Goal
Enforce that Vulkan renderer work stays native and never falls back to OpenGL by internal redirection.

## Changes
### 1) Policy rule in agent instructions
File: `AGENTS.md`

Added rule:
- Never redirect Vulkan renderer paths to OpenGL.
- All Vulkan renderer work (`rend_vk`, `vk_`/`pt_`) must be implemented natively.

### 2) Removed Vulkan-to-OpenGL redirection logic
File: `src/client/renderer.cpp`

Removed prior fallback path that forced OpenGL when:
- `r_renderer` requested Vulkan and
- `vk_rtx == 0`

Specifically removed:
- shadow-profile bridging alias table (`vk_*` -> `gl_*`)
- effective renderer override helper that returned `opengl`
- log message indicating OpenGL fallback

Renderer selection now uses normalized requested renderer directly.

## Resulting Behavior
- `r_renderer vulkan` always loads the Vulkan renderer module.
- `vk_rtx` no longer triggers backend switching.
- Any shadowmapping parity or quality work for Vulkan must be implemented in Vulkan paths.

## Validation
- Build command: `meson compile -C builddir`
- Expected: successful compilation with no OpenGL fallback selection logic left in `src/client/renderer.cpp`.
