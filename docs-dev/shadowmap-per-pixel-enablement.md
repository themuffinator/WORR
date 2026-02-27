# Shadowmap Per-Pixel Lighting Enablement

## Overview
Shadowmaps depend on the per-pixel lighting shader path to apply light and
shadowing to world surfaces and models. When `gl_per_pixel_lighting` is off,
shadowmaps previously rendered but were never applied, resulting in no visible
shadows on geometry.

## Change
- The renderer now enables the per-pixel lighting path when shadowmaps are
  requested and supported, regardless of `gl_per_pixel_lighting`.
- This ensures that dynamic lights and their shadowmaps are evaluated during
  the main world/entity pass.

## Rationale
Shadowmaps are only meaningful if the dynamic lighting shader runs. This
bridges the gap between the user-facing cvar and the shadowmap feature
so that `gl_shadowmaps 1` produces visible results without requiring an
additional cvar change.

Files: `src/rend_gl/main.c`.
