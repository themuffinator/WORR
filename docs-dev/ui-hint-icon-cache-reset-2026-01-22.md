# UI Hint Icon Cache Reset

## Summary
- Reset the UI hint icon cache on UI initialization to avoid stale image handles after renderer restarts.

## Issue
- The hint icon cache stores qhandle_t values for key icons.
- Renderer restarts invalidate image handles, but the cache persisted across UI shutdown/init.
- When a download overlay appeared after a restart, the cached handle could be out of range, triggering `IMG_ForHandle` assertions.

## Fix
- Added `UI_ResetBindIconCache()` in `src/game/cgame/ui/ui_hints.cpp`.
- Invoked the reset during `MenuSystem::Init()` so icons are re-registered after a renderer/filesystem restart.
