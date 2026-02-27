# Shadowmapping Dlight Culling Fixes - 2026-01-27

## Goals
- Fix shadow caster omissions for world and brush entities.
- Correct dlight/shadow culling so visible lights and occluders are not dropped.
- Keep renderer behavior compatible with legacy content and protocols.

## Changes
- Shadow pass BSP drawing now disables GL backface culling and avoids plane-based face culling so single-sided world/bmodel surfaces cast shadows.
- Shadow pass BSP faces now use the active entity transform (bmodel matrices) instead of forcing identity, fixing brush entity shadow placement.
- Dlight shadow visibility uses a light-volume leaf query (box vs PVS) instead of origin-only tests to avoid false negatives.
- Dlight culling for per-pixel uploads now uses influence radius from light origin to match shader falloff expectations.
- Shadow pass world marking uses the dlight influence radius centered on the light origin for consistent caster coverage.

## Affected Files
- src/rend_gl/gl.h
- src/rend_gl/main.c
- src/rend_gl/world.c
- src/rend_gl/tess.c
- src/rend_gl/shader.c
- src/rend_gl/sp3b.c

## Validation Notes
- Test dynamic lights near closed doors and moving platforms to confirm bmodels cast shadows in the correct world position.
- Verify world geometry casts shadows from lights behind single-sided surfaces.
- Check dlight visibility when the light origin is outside the camera PVS but its radius overlaps visible space.
