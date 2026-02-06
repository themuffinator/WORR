# Shadowmap Shadowlight Shader Filter Fix

## Problem

After enabling PVS2-expanded shadowlight visibility, maps could show incorrect
surface lighting as soon as shadowlights were active.

Observed behavior:

- Surfaces received light contributions that did not match expected occlusion.
- The issue appeared immediately when shadowlights were present, even when
  geometry should have blocked those lights.

## Root Cause

In the GLSL dlight upload path (`shader_load_lights`), shadow-designated static
lights (`DL_SHADOW_LIGHT`) were still uploaded when they did not receive a
shadowmap slot in the current frame (`glr.shadowmap_index[n] < 0`).

That meant they contributed as effectively unshadowed dynamic lights, which can
cause broad lighting leakage on world surfaces.

Affected files:

- `src/rend_gl/shader.c`
- `src/rend_gl/sp3b.c` (mirrored shader backend source)

## Fix

Added a guard in `shader_load_lights` to skip uploading `DL_SHADOW_LIGHT`
entries when all of the following are true:

1. shadowmaps are enabled,
2. shadowmap system is active (`gl_static.shadowmap_ok`),
3. the light has no assigned shadow slot (`glr.shadowmap_index[n] < 0`).

This keeps static shadowlights from falling back to unshadowed lighting in the
surface shader path.

## Why This Is Safe

- Dynamic lights (`DL_SHADOW_DYNAMIC`) are unaffected.
- If shadowmaps are disabled, no behavior change is introduced.
- Lights that are selected for shadowmap rendering continue to shade normally.

## Validation

1. Build succeeds with modified renderer sources.
2. Runtime check:
   - enter a map with shadowlights,
   - verify the previous full-level surface lighting artifacts no longer appear,
   - verify selected shadowlights still illuminate and cast shadows.
3. Debug check:
   - use `gl_shadow_draw_debug 1`,
   - confirm only slot-assigned static shadowlights contribute in shader lighting.

