# Soft Shadow Mapping (GL)

## Overview
Soft shadow mapping adds per-pixel shadowing for point shadowlights and
(optional) dynamic lights in the OpenGL shader renderer. Shadow maps are
rendered into a texture array (six faces per light) and sampled with
selectable filters (PCF, VCM) for soft edges.

## Shadowlight Pipeline (sgame -> client)
- Shadowlight data is stored in `shadow_light_data_t` and populated from map
  spawn keys in `src/game/sgame/g_spawn.cpp` (e.g. radius, intensity, color,
  cone angle, resolution).
- `setup_dynamic_light` in `src/game/sgame/g_misc.cpp` marks entities with
  `RF_CASTSHADOW` and stores the `shadowlightinfo` used by the client.
- `setup_shadow_lights` builds the `CS_SHADOWLIGHTS` configstrings that are
  parsed on the client.
- `CL_ParseShadowLights` in `src/client/precache.c` fills `cl.shadowdefs`.
- `CL_AddShadowLights` in `src/client/effects.c` calls `V_AddLightEx`, which
  tags point shadowlights with `DL_SHADOW_LIGHT` when `resolution > 0`.

## GL Shadow Map Pipeline
- A single `GL_TEXTURE_2D_ARRAY` stores shadow depth for all lights; each light
  consumes six layers (cubemap faces). A dedicated FBO and depth renderbuffer
  are reused for all layers.
- The shadowmap format stores depth and depth-squared (RG16F) for variance
  filtering when using VCM.
- `GL_RenderShadowMaps` chooses shadow-casting lights per frame:
  - Shadowlights (`DL_SHADOW_LIGHT`) are always prioritized.
  - Dynamic lights (`DL_SHADOW_DYNAMIC`) are included when
    `gl_shadowmap_dynamic` is enabled.
  - Only point lights (`conecos == 0`) are eligible.
- For each light and face, the world and opaque entities are rendered with the
  `GLS_SHADOWMAP` shader variant. Depth is stored as
  `distance_to_light / light_radius` in the color buffer.
- The main lighting shader samples the array using the filter selected by
  `gl_shadowmap_filter` and applies the result to the dynamic light
  contribution. The per-light shadow slot is provided via `shadowmap_index`
  in the dynamic light UBO.

## Cvars
`gl_shadowmaps`
- `0` disables shadow mapping.
- `1` enables shadow mapping.
- Default is `1` (on).

`gl_shadowmap_size`
- Shadowmap face resolution (square).
- Default is `512`.

`gl_shadowmap_lights`
- Maximum number of shadow-casting lights to render per frame (6 layers each).
- Default is `2`.

`gl_shadowmap_dynamic`
- `0` shadows only shadowlights.
- `1` shadows both shadowlights and dynamic lights.
- Default is `1` (on).

`gl_shadowmap_bias`
- Depth bias in normalized shadow space (distance / radius).
- Default is `0.005`.

`gl_shadowmap_softness`
- Softness scale for PCF sampling (0 disables filtering).
- Default is `1`.

`gl_shadowmap_filter`
- `0` hard comparison (single sample).
- `1` PCF (percentage-closer filtering).
- `2` VCM (variance cube mapping).
- Default is `1` (PCF).

`gl_shadowmap_quality`
- `0` to `3` quality levels.
- PCF: `0` = 1 tap, `1` = 4 taps, `2` = 9 taps, `3` = 16 taps.
- VCM: higher values reduce light bleeding at additional cost.
- Default is `1`.

## Notes and Limitations
- Only point lights cast shadows (spot/cone lights are excluded).
- Alpha/translucent surfaces and sprites do not cast shadows.
- Viewmodels (`RF_WEAPONMODEL`) are skipped in the shadow pass.
- The resolution and max light count are global; use the cvars to tune
  performance and quality.
- Shadow maps require the shader GL backend and GL 3.0 / ES 3.0 support.
