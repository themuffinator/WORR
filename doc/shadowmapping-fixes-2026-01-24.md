# Shadowmapping Fixes (Phase 1-2) - 2026-01-24

## Goals
- Fix incorrect shadow caster selection and world occluder culling.
- Ensure static shadowmap caching respects dynamic casters.
- Ensure map entity shadowlights compete fairly with effect dlights.
- Provide light origin safety for shadow cameras inside solid.

## Changes

### Phase 1 - Correctness
- Added a shadow-only local player entity in first-person so the player still casts shadows.
- Normal rendering now skips RF_VIEWERMODEL while shadow passes still render it.
- Bmodel shadow range now uses bounds center transformed by entity rotation/scale; radius uses bounds extents.
- Shadow pass world marking no longer uses camera PVS. BSP nodes are marked by light-volume intersection.
- Added gl_shadowmap_cache_mode and per-light caster invalidation:
  - 0: no caching
  - 1: cache only if no casters intersect the light
  - 2: hash casters within light range to decide invalidation
- Lights inside solid are shifted to a safe shadow origin; the adjusted origin is used for shading and shadow mapping.

### Phase 2 - Light Selection
- Shadowmap light selection now scores all shadow-capable lights by intensity, radius, and distance. Dynamic lights receive a small boost.
- Map dynamic_light entities are no longer capped by PVS count; the top N lights are chosen by score.
- Dlight selection now keeps the top N lights by score across all sources; later higher-score lights replace lower ones when full.

## New or Changed CVars
- gl_shadowmap_cache_mode (int): 0 disable, 1 world-only caching, 2 hash-based caching.

## Affected Files
- src/client/entities.cpp
- src/client/effects.cpp
- src/client/view.cpp
- src/rend_gl/main.c
- src/rend_gl/world.c
- src/rend_gl/gl.h
- src/rend_gl/texture.c

## Validation Notes
- Test maps with dynamic_light entities (spot and point) and moving bmodels.
- Verify first-person player shadow appears and tracks player movement.
- Toggle gl_shadowmap_cache_mode 0/1/2 and confirm static light shadows update appropriately.
- Compare visible map shadowlights near the camera before and after the change.
