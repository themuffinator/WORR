# Shadowmap dlight retention and defaults

## Overview
These changes keep shadowmap-capable dynamic lights active even when vertex
lighting or disabled gl_dynamic would otherwise clear them, and add a safe
fallback so map shadowlights without an explicit resolution still cast.

## Dlight retention with shadowmaps
- Shadowmaps now keep refdef dlights alive even when gl_dynamic is off or
  gl_vertexlight is on, so shadowmap selection still sees dynamic lights.
- This path only changes behavior when shadowmaps are supported and enabled.

Files: `src/rend_gl/main.c`.

## Shadowlight defaults for map entities
- Shadowlights with a radius but no explicit `shadowlightresolution` key now
  default to a non-zero resolution so they can cast shadows.
- Explicit `shadowlightresolution 0` still disables shadow casting.
- Shadowlights without an explicit `shadowlightstyle` now inherit the entity
  `style` when provided, preserving flicker styles.

Files: `src/game/sgame/g_misc.cpp`.
