# Renderer Build Fixes (2026-01-19)

## Overview
- Resolved renderer DLL build errors by removing direct engine symbol dependencies in the Vulkan VKPT module.
- Added missing OpenGL loader entry for `glReadBuffer` used by the bloom mipmap path.

## Details
### Vulkan VKPT shader load path
- `src/rend_vk/vkpt/main.c` now reads the user files basedir through the `homedir` cvar via renderer import.
- Removed the direct reference to `sys_homedir`, which is not exported to renderer DLLs.
- Path normalization for this lookup relies on forward slashes; `FS_ReplaceSeparators` is no longer used to avoid unresolved symbols.

### OpenGL function loader
- `src/rend_gl/qgl.h` declares `qglReadBuffer`.
- `src/rend_gl/qgl.c` loads `glReadBuffer` for desktop GL 1.1 and ES 3.0 contexts.
- This unblocks compilation of `src/rend_gl/main.c` where bloom mipmap generation uses `qglReadBuffer` when available.

## Notes
- Behavior is unchanged for shader search order; the adjustment only ensures renderer DLL linkage.
- Warnings remain in several Vulkan sources but do not block the build.
