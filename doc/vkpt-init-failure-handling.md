# VKPT initialization failure handling

This document describes the startup changes made to avoid debug-time assertion crashes when a vkpt subsystem fails to initialize and to expose which subsystem failed.

## Problem
`vkpt_initialize_all()` previously asserted on any subsystem initialization failure before emitting a readable error. In debug builds this produced an assertion dialog and blocked useful diagnostics, even for recoverable errors such as missing shader modules or resource failures.

## Changes
- `src/rend_vk/vkpt/main.c`: `vkpt_initialize_all()` now records the `VkResult` from each init callback, logs the subsystem name on failure, and returns an error code instead of asserting.
- `src/rend_vk/vkpt/main.c`: `R_Init()` now checks the return values of the initialization passes (default, reload‑shader, swapchain recreate) and cleanly tears down the renderer if any pass fails.

## Result
Initialization failures now report the exact failing subsystem (e.g. `textures`, `shadowmap`, `pt`) and exit gracefully without a debugger-break assertion.
## Error reporting
- `vkpt_initialize_all()` now sets the renderer last-error string to the failing subsystem name and `VkResult`, so the client fatal error dialog reports the exact failing vkpt stage.
