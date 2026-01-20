# Shadowmap Improvements Implementation

## Summary
This document records the renderer-side changes applied to the shadowmap plan,
including caching, spot light shadows, and the higher light cap.

## Shadowmap caching
- Added a cache texture array that mirrors the primary shadowmap array.
- Static shadow lights (DL_SHADOW_LIGHT) now reuse cached faces when clean.
- Cache invalidation is driven by static light origin changes and cvar toggles.
- Cache copy uses glCopyTexSubImage3D to move data between arrays.
- New cvar: gl_shadowmap_cache (default 1) controls whether caching is used.

## Spot light shadow support
- Cone lights are now allowed to cast shadows if they have a valid resolution.
- Spot lights render a single shadowmap face using the cone direction.
- Spot projection FOV is derived from the light cone angle (2 * cone angle).
- Sampling uses a perspective projection based on the cone cosine.
- Spot lights currently use the first layer of their slot (slot * 6).

## Per-face frustum culling
- Shadow passes already build frustum planes per face through GL_SetupFrustum.
- Spot shadow passes now use the cone FOV in that frustum setup.

## Shadowmap light cap
- MAX_SHADOWMAP_LIGHTS increased to 16.
- Runtime limit remains gl_shadowmap_lights (default 2) and GPU layer caps.

## Manual verification
1. Build: meson compile -C builddir
2. Load a map with shadow-casting lights and confirm shadows render.
3. Toggle caching:
   - gl_shadowmap_cache 1: static shadows should persist without re-render.
   - gl_shadowmap_cache 0: visuals should remain correct (full render).
4. Spotlights:
   - Create a spotlight with shadowlightradius and ensure it casts a cone shadow.
   - Verify shadows respect the cone direction and falloff.
5. Light cap:
   - Set gl_shadowmap_lights to 8 or higher and confirm multiple lights render.
