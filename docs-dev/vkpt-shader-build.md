# VKPT shader compilation and startup guard

This document covers the shader compilation flow added for the Q2RTX vkpt port in WORR and the startup guard that prevents Vulkan initialization from crashing when SPIR-V modules are missing.

## Problem
The Vulkan renderer loads SPIR-V modules from `shader_vkpt/*.spv`. When these files are missing, `vkpt_load_shader_modules()` returns null handles and subsequent pipeline creation (for example the normal-map normalization compute pipeline) can crash in the driver.

## Shader build pipeline
- Added a Python build tool that compiles vkpt shader sources to SPIR-V using `glslangValidator`.
- The build target writes output into `builddir/baseq2/shader_vkpt`, matching the filesystem lookup path used by `FS_LoadFile` during renderer initialization.
- The compile target is built by default when Vulkan is enabled so that `meson compile -C builddir` produces the shader cache alongside the renderer DLLs.

Files:
- `tools/compile_vkpt_shaders.py`: compiles all `.comp/.vert/.frag/.rgen/.rchit/.rahit/.rmiss/.rint` sources and emits `.spv` output (including `.pipeline` and `.query` variants for ray tracing).
- `meson.build`: adds the `vkpt_shaders` custom target and wires its output directory to the configured `base-game` path.

## Startup guard
- `src/rend_vk/vkpt/main.c`: `R_Init` now aborts Vulkan initialization with a clear error message when shader modules are missing, preventing undefined driver behavior.
- `src/rend_vk/vkpt/main.c`: `vkpt_reload_shader` now halts pipeline reinitialization when shader loading fails.
