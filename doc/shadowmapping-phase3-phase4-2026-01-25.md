# Shadowmapping Phase 3-4 (Filtering + CSM) - 2026-01-25

## Goals
- Add bias/offset controls and filtering paths for shadowmaps (PCF/VSM/EVSM/PCSS).
- Add a directional sun shadow system with cascaded shadow maps (CSM).

## Phase 3 - Bias + Filtering Paths
- Added slope-scale bias and normal offset for receivers to reduce acne and peter-panning.
- Added VSM and EVSM modes with configurable bleed and minimum variance.
- Added PCSS mode with blocker and filter sample counts.
- Added optional VSM/EVSM mipmap generation and LOD bias driven by shadow softness.
- Shadowmap sampling is now reconfigured at runtime when filter/mip/softness changes.

### New or Updated CVars
- `gl_shadow_bias_slope` (float): slope-scale bias component for shadow compare.
- `gl_shadow_normal_offset` (float): receiver normal offset for shadow compare.
- `gl_shadow_vsm_bleed` (float): VSM/EVSM bleed reduction threshold.
- `gl_shadow_vsm_min_variance` (float): VSM/EVSM minimum variance clamp.
- `gl_shadow_evsm_exponent` (float): EVSM exponent for depth warping.
- `gl_shadow_vsm_mipmaps` (int): enable mipmaps for VSM/EVSM shadowmaps.
- `gl_shadow_pcss_blocker_samples` (int): blocker sample count for PCSS.
- `gl_shadow_pcss_filter_samples` (int): filter sample count for PCSS.

### Filter Modes
- `gl_shadowmap_filter`:
  - 0: hard compare
  - 1: PCF
  - 2: VSM
  - 3: EVSM
  - 4: PCSS

## Phase 4 - CSM / Directional Shadow System
- Added cascaded shadow maps (up to 4 cascades) using a 2D array texture and per-cascade orthographic projections.
- Sun shadows are rendered in a dedicated shadow pass before point/spot shadowmaps.
- Cascade splits are computed from camera frustum using lambda blend (log/linear).
- Cascade stabilization snaps the light-space center to texels to reduce shimmer.
- Sun lighting is applied in the dynamic lighting path and uses the same filter mode as local lights.

### New CVars
- `gl_sun_enable` (int): enable sun directional shadows.
- `gl_sun_dir` (vec3): direction to the sun (world -> sun).
- `gl_sun_color` (color/vec3): sun light color.
- `gl_sun_intensity` (float): sun light intensity multiplier.
- `gl_csm_cascades` (int): number of cascades (1-4).
- `gl_csm_lambda` (float): split distribution (0 = uniform, 1 = log).
- `gl_csm_resolution` (int): per-cascade shadowmap size.

## Implementation Notes
- Sun shadows use AABB-based BSP node marking to avoid PVS clipping in the shadow pass.
- Sun shadow depth uses `gl_FragCoord.z` for orthographic projection storage.
- CSM sampling uses view-space depth against split distances; cascades do not blend.
- Shadowmap filtering + mipmaps apply to both local and sun shadowmaps.

## Affected Files
- `src/rend_gl/gl.h`
- `src/rend_gl/main.c`
- `src/rend_gl/shader.c`
- `src/rend_gl/state.c`
- `src/rend_gl/texture.c`
- `src/rend_gl/world.c`

## Validation Notes
- Toggle `gl_sun_enable 1` and verify cascades update with camera movement.
- Compare `gl_shadowmap_filter` modes on both local lights and sun shadows.
- Test VSM/EVSM with `gl_shadow_vsm_mipmaps 1` and various `gl_shadowmap_softness`.
- Check cascade split tuning with `gl_csm_lambda` and `gl_csm_cascades`.
