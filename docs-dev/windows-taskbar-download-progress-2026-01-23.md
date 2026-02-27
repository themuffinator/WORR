# Windows taskbar download progress (2026-01-23)

## Overview
- Added taskbar progress feedback for client downloads on modern Windows.
- Reuses existing download queue state without altering protocols or file formats.

## Behavior
- Shows a normal taskbar progress bar when a current download reports a non-zero percentage.
- Switches to an indeterminate taskbar bar while downloads are queued or total size is unknown.
- Clears the taskbar progress indicator when no downloads are pending.

## Implementation details
- `src/client/main.cpp` maps download queue state to a taskbar state each frame.
- `inc/system/system.h` defines `sys_taskbar_progress_t` plus `Sys_SetTaskbarProgress`.
- `src/windows/system.c` implements a lazy COM-backed `ITaskbarList3` helper with
  state caching to avoid redundant COM calls; cleanup occurs in `Sys_Quit`.
- `src/unix/system.c` provides a no-op stub for non-Windows clients.
- `src/windows/meson.build` links `ole32` and `uuid` for COM support.

## Compatibility
- Download protocol and legacy server/demo support remain unchanged.

## Testing
- Not run (Windows client download flow required).
