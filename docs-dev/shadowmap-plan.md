# Shadow Mapping Improvements - Implementation Plan

## Overview

This plan covers four priority improvements to the GL renderer shadow mapping system:
1. Shadow caching for static lights
2. Frustum culling per shadow face
3. Increase MAX_SHADOWMAP_LIGHTS
4. Spot light shadow support

---

## Priority 1: Shadow Caching for Static Lights

Static shadowlights (`DL_SHADOW_LIGHT`) have fixed positions - no need to re-render their shadows every frame.

### Proposed Changes

---

#### [MODIFY] [gl.h](../src/rend_gl/gl.h)

Add cached shadow texture and dirty tracking to `glStatic_t`:

```c
// After shadowmap_depth (line ~129):
GLuint          shadowmap_cache_tex;    // Cached static light shadows
bool            shadowmap_cache_dirty[MAX_SHADOWMAP_LIGHTS * SHADOWMAP_FACE_COUNT];
uint32_t        shadowmap_cache_frame;  // Frame when cache was last valid
vec3_t          shadowmap_cache_origins[MAX_SHADOWMAP_LIGHTS]; // Cached light origins
```

---

#### [MODIFY] [texture.c](../src/rend_gl/texture.c)

1. In `GL_InitShadowmapsFormat()` (~line 1711): Create a second 2D texture array for the cache with same format/size.

2. In `GL_ShutdownShadowmaps()` (~line 1790): Delete the cache texture.

---

#### [MODIFY] [main.c](../src/rend_gl/main.c)

1. In `GL_SelectShadowLights()` (~line 1342): Track which static lights changed position:
   ```c
   // For each DL_SHADOW_LIGHT:
   if (!VectorCompare(dl->origin, gl_static.shadowmap_cache_origins[slot])) {
       VectorCopy(dl->origin, gl_static.shadowmap_cache_origins[slot]);
       for (int f = 0; f < 6; f++)
           gl_static.shadowmap_cache_dirty[slot * 6 + f] = true;
   }
   ```

2. In `GL_RenderShadowMaps()` (~line 1387): Skip rendering for clean cached faces:
   ```c
   for (int face = 0; face < SHADOWMAP_FACE_COUNT; face++) {
       int layer = light_slot * SHADOWMAP_FACE_COUNT + face;
       
       // Skip if static light and cache is clean
       if (dl->shadow == DL_SHADOW_LIGHT && !gl_static.shadowmap_cache_dirty[layer]) {
           // Copy from cache texture to main texture
           continue;
       }
       
       // ... existing render code ...
       
       // After render, copy to cache if static light
       if (dl->shadow == DL_SHADOW_LIGHT) {
           // glCopyImageSubData or render-to-cache
           gl_static.shadowmap_cache_dirty[layer] = false;
       }
   }
   ```

3. Add `gl_shadowmap_cache` cvar (default 1) to enable/disable caching.

---

#### [NEW] [CVar] `gl_shadowmap_cache`

| Name | Default | Description |
|------|---------|-------------|
| `gl_shadowmap_cache` | 1 | Cache static light shadows (0=off, 1=on) |

---

## Priority 2: Frustum Culling Per Shadow Face

Currently the full BSP tree is traversed for each shadow face. We can cull the 90° pyramid frustum per face.

### Proposed Changes

---

#### [MODIFY] [main.c](../src/rend_gl/main.c)

1. Add new function to build shadow frustum planes:
   ```c
   static void GL_SetupShadowFrustum(const vec3_t origin, const vec3_t axis[3], float radius)
   {
       // Build 4 frustum planes for 90° FOV centered on `axis[0]`
       // Similar to GL_SetupFrustum() but simpler since FOV is always 90°
       float sf = 0.707107f;  // sin(45°)
       float cf = 0.707107f;  // cos(45°)
       
       vec3_t forward, left, up;
       VectorScale(axis[0], sf, forward);
       VectorScale(axis[1], cf, left);
       VectorScale(axis[2], cf, up);
       
       // Build planes: left, right, top, bottom
       VectorAdd(forward, left, glr.frustumPlanes[0].normal);
       VectorSubtract(forward, left, glr.frustumPlanes[1].normal);
       VectorAdd(forward, up, glr.frustumPlanes[2].normal);
       VectorSubtract(forward, up, glr.frustumPlanes[3].normal);
       
       for (int i = 0; i < 4; i++) {
           glr.frustumPlanes[i].dist = DotProduct(origin, glr.frustumPlanes[i].normal);
           glr.frustumPlanes[i].type = PLANE_NON_AXIAL;
           SetPlaneSignbits(&glr.frustumPlanes[i]);
       }
   }
   ```

2. In `GL_RenderShadowMaps()` loop, call `GL_SetupShadowFrustum()` after `GL_SetupShadowView()`:
   ```c
   GL_SetupShadowView(dl->origin, gl_shadow_face_axis[face], radius);
   GL_SetupShadowFrustum(dl->origin, gl_shadow_face_axis[face], radius);
   ```

---

## Priority 3: Increase MAX_SHADOWMAP_LIGHTS

### Proposed Changes

---

#### [MODIFY] [gl.h](../src/rend_gl/gl.h)

Change compile-time limit:
```diff
-#define MAX_SHADOWMAP_LIGHTS    4
+#define MAX_SHADOWMAP_LIGHTS    16
```

---

#### [MODIFY] [main.c](../src/rend_gl/main.c)

Update default cvar value if desired (currently `gl_shadowmap_lights` defaults to 2, max clamped to `MAX_SHADOWMAP_LIGHTS`).

---

> [!NOTE]
> The runtime limit is still controlled by `gl_shadowmap_lights` cvar, so increasing the compile-time max doesn't force higher usage. Users can tune based on their hardware.

---

## Priority 4: Spot Light Shadow Support

Spot/cone lights currently excluded (`conecos != 0`). Add single-face shadow projection for them.

### Proposed Changes

---

#### [MODIFY] [gl.h](../src/rend_gl/gl.h)

Add spot light shadow storage - use same texture array, 1 layer per spot light instead of 6:
```c
// After shadowmap_index in glRefdef_t (~line 202):
bool            shadowmap_is_spot[MAX_DLIGHTS];  // True if using single-face projection
```

---

#### [MODIFY] [main.c](../src/rend_gl/main.c)

1. In `GL_SelectShadowLights()`: Allow spot lights:
   ```diff
   -if (dl->conecos != 0.0f || dl->radius <= 0.0f || dl->intensity <= 0.0f)
   +if (dl->radius <= 0.0f || dl->intensity <= 0.0f)
       continue;
   +
   +glr.shadowmap_is_spot[i] = (dl->conecos != 0.0f);
   ```

2. In `GL_RenderShadowMaps()`: For spot lights, render only 1 face using spot direction:
   ```c
   int face_count = glr.shadowmap_is_spot[light_index] ? 1 : SHADOWMAP_FACE_COUNT;
   
   for (int face = 0; face < face_count; face++) {
       // For spots: use dl->spot.direction as forward axis
       vec3_t spot_axis[3];
       if (glr.shadowmap_is_spot[light_index]) {
           VectorCopy(dl->spot.direction, spot_axis[0]);
           PerpendicularVector(spot_axis[1], spot_axis[0]);
           CrossProduct(spot_axis[0], spot_axis[1], spot_axis[2]);
           VectorNormalize(spot_axis[1]);
           VectorNormalize(spot_axis[2]);
       }
       
       const vec3_t (*axis)[3] = glr.shadowmap_is_spot[light_index] 
           ? &spot_axis 
           : &gl_shadow_face_axis[face];
       
       // ... rest of render code using *axis ...
   }
   ```

3. Add spot-specific frustum with cone angle FOV instead of 90°.

---

#### [MODIFY] [shader.c](../src/rend_gl/shader.c)

1. Update `shadow_point()` (~line 249) to handle spot lights:
   ```glsl
   float shadow_spot(vec3 light_dir, float dist, float radius, float shadow_index, 
                     vec3 spot_dir, float spot_angle) {
       if (shadow_index < 0.0 || u_shadow_params.x <= 0.0)
           return 1.0;
       
       // Project onto spot shadow map (single face, arbitrary direction)
       vec3 dir = light_dir / max(dist, 1.0);
       
       // Build UV from direction projected onto spot plane
       vec3 perp1 = normalize(cross(spot_dir, vec3(0,0,1)));
       if (length(perp1) < 0.01)
           perp1 = normalize(cross(spot_dir, vec3(0,1,0)));
       vec3 perp2 = cross(spot_dir, perp1);
       
       float u = dot(dir, perp1) * 0.5 + 0.5;
       float v = dot(dir, perp2) * 0.5 + 0.5;
       
       float layer = shadow_index;
       float depth = dist / radius;
       // ... same filtering as shadow_point ...
   }
   ```

2. Update `calc_dynamic_lights()` to call `shadow_spot()` when cone light detected:
   ```glsl
   if (light_cone != 0.0) {
       result *= shadow_spot(shadow_vec, shadow_dist, dlights[i].radius, 
                             dlights[i].shadow.x, dlights[i].cone.xyz, light_cone);
   } else {
       result *= shadow_point(shadow_vec, shadow_dist, dlights[i].radius, 
                              dlights[i].shadow.x);
   }
   ```

---

#### [MODIFY] [dlight UBO structure in shader.c](../src/rend_gl/shader.c#L139-L146)

Add spot-specific fields to shader dlight struct:
```glsl
struct dlight_t {
    vec3    position;
    float   radius;
    vec4    color;
    vec4    cone;       // xyz = direction, w = angle
    vec4    shadow;     // x = shadow_index, y = is_spot, z/w = reserved
};
```

---

## Summary of File Changes

| File | Changes |
|------|---------|
| [gl.h](../src/rend_gl/gl.h) | Cache texture + dirty flags, increase MAX_SHADOWMAP_LIGHTS, spot tracking |
| [texture.c](../src/rend_gl/texture.c) | Create/destroy cache texture |
| [main.c](../src/rend_gl/main.c) | Cache logic, frustum culling, spot light selection, new cvar |
| [shader.c](../src/rend_gl/shader.c) | shadow_spot() function, updated dlight struct |

---

## Verification Plan

> [!IMPORTANT]
> No automated rendering tests exist in this codebase. Verification is manual visual inspection.

### Manual Testing Steps

#### Test Environment Setup
1. Build the project with `meson compile -C builddir`
2. Launch game with a test map containing shadow-casting lights (e.g. any map with `light` entities that have `shadowlightradius` key)

#### Priority 1: Shadow Caching
1. Place a static shadowlight in a map and observe shadows
2. Add `gl_shadowmap_cache 1` (default) → static light shadows should persist correctly
3. Toggle `gl_shadowmap_cache 0` → no visual change (shadows still work via full render)
4. Use `r_speeds 1` or similar to verify fewer draw calls on subsequent frames with caching enabled
5. Move player around the static light → shadows remain stable

#### Priority 2: Frustum Culling
1. Load a map with a shadowlight near a corner (geometry only visible from some cube faces)
2. Observe that shadow quality and appearance remain unchanged
3. Verify via `r_speeds` that fewer primitives are rendered during shadow passes

#### Priority 3: Increased Light Limit
1. Set `gl_shadowmap_lights 8` or higher
2. Place 8+ shadow-casting lights in view
3. Verify all 8 lights cast correct shadows
4. Ensure no crashes or visual artifacts

#### Priority 4: Spot Light Shadows
1. Create a spot light with `spotlight` key and `shadowlightradius`
2. Verify spot light casts directional shadow (cone-shaped, not omnidirectional)
3. Walk into/out of the spot cone → shadow should only affect cone area
4. Compare with point light to confirm different shadow shape
