# .install Staging + Asset Packaging Workflow (2026-02-19)

## Summary
Build output staging is now standardized on `.install/` for local distributables.
After each build, `.install/` is refreshed with current binaries/game DLLs and a packaged asset archive.

## Rule
- `.install/` is the canonical local distributable root for development builds.
- Build workflows must repopulate `.install/` at the end of each build.
- Repo assets from `assets/` must be packaged into `baseq2/worr-assets.pkz` under `.install/`.

## Build/Stage pipeline
The VS Code default build task (`meson: build`) now runs in sequence:

1. `meson: setup`
   - Creates or reconfigures `builddir`.
2. `meson: compile`
3. `install: stage (.install)`
   - Runs `tools/stage_install.py`.
   - Clears and repopulates `.install/` from `builddir` runtime binaries.
   - Copies `builddir/baseq2` (game DLLs + shader cache) into `.install/baseq2`.
4. `assets: package (.install)`
   - Runs `tools/package_assets.py` and writes:
   - `.install/baseq2/worr-assets.pkz`

## Launch behavior
All launch configs now execute from `.install/`:
- `.install/worr.exe`
- `.install/worr.ded.exe`

Each launch config uses `preLaunchTask: "meson: build"` so debug runs use freshly staged distributables.

## Build unblocker applied
During validation, the Vulkan shader compile helper expected a legacy path:
- `src/rend_vk/vkpt/shader`

Current source layout uses:
- `src/rend_rtx/vkpt/shader`

`tools/compile_vkpt_shaders.py` now resolves VKPT shader roots with:
1. `src/rend_rtx/vkpt` (preferred)
2. `src/rend_vk/vkpt` (fallback)

This keeps Vulkan shader compilation working across both layouts.

## Files changed
- `AGENTS.md`: added `.install/` workflow rule.
- `.vscode/tasks.json`: added setup/reconfigure + runtime staging + asset packaging tasks.
- `.vscode/launch.json`: switched executable paths/cwd to `.install/`.
- `.gitignore`: added `/.install/` ignore entry.
- `tools/stage_install.py`: new runtime staging script for `.install/`.
- `tools/package_assets.py`: new asset archive packager.
- `tools/compile_vkpt_shaders.py`: fixed VKPT shader source path resolution.
