# Windows/SDL Fullscreen + Display Selection Updates (2026-01-25)

## Overview
- Added archived cvars to control SDL display selection and Windows exclusive vs. borderless fullscreen.
- Updated SDL display handling to align modelists and fullscreen transitions with the selected/active display.
- Added a Win32 borderless fullscreen path to improve Windows 11 capture compatibility.

## New/Updated Cvars
- `r_display` (archive): SDL display selection. `0` = auto (current window display), `1+` = 1-based display index, or a display name match.
- `r_fullscreen_exclusive` (archive): `1` = exclusive fullscreen (default), `0` = borderless fullscreen.

## SDL Updates
- `src/unix/video/sdl.c` now resolves a display ID from `r_display` and applies it consistently for the mode list and fullscreen switching.
- Window display changes refresh geometry, update the modelist, and re-apply mode sizing to keep `R_ModeChanged`/`SCR_ModeChanged` aligned.
- Invalid display selections warn and fall back to the primary display.

## Windows Updates
- `src/windows/client.c` adds a borderless fullscreen path using monitor bounds and skipping `ChangeDisplaySettings`.
- Focus switch flips only apply to exclusive fullscreen to avoid desktop mode churn in borderless.

## UI Updates
- Video menu adds `r_fullscreen_exclusive` (exclusive vs. borderless) and a `r_display` entry for manual multiscreen selection.

## Compatibility Notes
- Exclusive fullscreen remains the default to preserve legacy behavior.
- No changes to protocol, demo playback, or legacy server compatibility.
