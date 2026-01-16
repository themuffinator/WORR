# Cgame Draw Migration Notes

## Summary
- Renamed `src/game/cgame/cg_screen.cpp` to `src/game/cgame/cg_draw.cpp` to mirror Quake 3-style draw module naming.
- `CG_DrawHUD` remains the draw entry point and still handles HUD layouts, centerprints, notify, inventory, weapon bar, and wheel.

## Migration Targets From `src/client/screen.c`

### Suitable With Minimal Engine Changes
- Crosshair color/pulse logic and pickup pulse tracking can live in cgame using `ps->stats` plus local cvar state.
- Chat HUD rendering (line buffer + fade) can move once chat lines are forwarded into cgame.

### Requires New Imports/Exports or Data Plumbing
- Hit markers: engine owns `cl.hit_marker_*` and triggers sound in `CL_AddHitMarker`; cgame needs a hit event export or must track `STAT_HIT_MARKER` changes itself.
- Damage indicators: depends on `cl.predicted_angles` and `R_DrawStretchRotatePic`; needs imports for predicted view angles and rotated sprite drawing.
- POIs: depends on `cl.refdef` and palette conversion; needs a world-to-screen helper import plus RGBA color data or palette access.
- Crosshair hit feedback uses `cl.crosshair_hit_time`/`cl.crosshair_hit_damage`; the timing/damage state should migrate into cgame or be exported.

### Engine-Owned (Keep In `src/client/screen.c`)
- Viewsize/virtual screen scaling, loading plaque, pause overlay, demo bar, netgraph/lagometer, debug graphs.
- Draw-string registry (`draw`/`undraw`) and console integration.
