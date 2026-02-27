# Shadowmapping HOM Fix - 2026-01-24

## Summary
- Fixed hall-of-mirrors artifacts caused by shadow-pass visibility marking leaking into the main view.

## Root Cause
Shadow passes increment `glr.visframe` and stamp `node->visframe` based on light-volume intersections. If the main view’s PVS does not change (common), `GL_MarkLeaves` early-outs and never re-stamps visible nodes for the camera. The main pass then renders with shadow-pass visibility, leaving large regions of the framebuffer untouched (HOM).

## Fix
- Restore `glr.visframe` after shadow rendering.
- Force the main view to rebuild PVS by invalidating `glr.viewcluster1/2` after shadow passes.

## Files
- `src/rend_gl/main.c`

## Validation
- Render shadowmaps with multiple light snapshots; verify world surfaces outside light volumes are fully drawn each frame.
- Toggle camera position without changing clusters to ensure PVS is still rebuilt after shadow pass.
