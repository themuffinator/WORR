# Cgame HUD FPS Display (2026-01-22)

## Goal
Provide a cgame-side FPS overlay in `cg_draw.cpp` that mirrors Q3-style HUD
logic and can be toggled independently via a new cvar.

## What changed
- Added `cg_draw_fps` (archived cvar) to enable/disable the FPS display.
- Implemented a lightweight FPS counter driven by `CG_DrawHUD` using
  `CL_ClientRealTime` and a once-per-second update cadence.
- Rendered the value as `${fps_value} fps` at the top-right of the safe HUD
  area via `SCR_DrawFontString` with right alignment.

## Behavior notes
- FPS counting is updated only on the primary split (`isplit == 0`) to avoid
  double-counting in splitscreen; the cached value is drawn for all splits.
- No server or protocol changes; this is purely a client HUD overlay.
