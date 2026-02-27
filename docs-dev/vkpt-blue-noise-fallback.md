# VKPT blue-noise fallback

This document describes the fallback added for missing blue-noise textures during vkpt texture initialization.

## Problem
`vkpt_textures_initialize()` fails when the blue-noise PNGs are missing from the filesystem. This stops the Vulkan renderer even when the rest of the pipeline can run.

## Change
- `src/rend_vk/vkpt/textures.c`: `load_blue_noise()` now generates a deterministic procedural noise fallback when the on-disk blue-noise PNGs are absent.
- The fallback is logged once as a warning so missing assets are visible without aborting initialization.
- Buffer creation and mapping errors now return explicit `VkResult` failures.

## Notes
- The procedural fallback is intended to keep the renderer running when `blue_noise.pkz` is not present.
- For parity with Q2RTX, provide the original `blue_noise.pkz` in `baseq2/` so the renderer uses the authored blue-noise textures.
