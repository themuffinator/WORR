# Depth-of-Field Slow-Time Transition

## Overview
- Adds a depth-of-field (DOF) screen transition that ramps with slow-time activation and deactivation.
- Slow-time sources include the weapon/powerup wheel (timescale ramp) and the help/inventory layouts.
- The DOF effect is driven per-frame by `refdef_t::dof_strength` and only rendered when it is > 0.

## Cvars
- `r_dof` enables DOF rendering (default `1`, flags `CVAR_ARCHIVE | CVAR_LATCH`).
- `r_dofBlurRange` overrides the blur range in world units (default `0.0`, flags `CVAR_SERVERINFO`).
- `r_dofFocusDistance` overrides the focus distance in world units (default `0.0`, flags `CVAR_SERVERINFO`).
- When either override is `0.0`, the shader uses an auto value (focus from center depth, range from focus).

## Slow-Time Driver
- `src/client/view.cpp` computes `dof_strength` as the max of:
  - Wheel ramp: derived from `CL_Wheel_TimeScale()`.
  - Layout ramp: smoothed blend when `LAYOUTS_INVENTORY` or `LAYOUTS_HELP` are active.
- The transition uses a frame-time based ease to avoid popping in/out.

## Renderer Pipeline
- `r_dof` enables a depth texture for the main scene framebuffer (`TEXNUM_PP_DEPTH`).
- A new GLSL path (`GLS_DOF`) mixes the scene and a blurred copy based on depth vs. focus distance.
- DOF pass sequence:
  1. Render scene to `FBO_SCENE` (color + depth).
  2. Blur the scene into `PP_BLUR_0/1` (quarter res).
  3. Composite DOF to the backbuffer with optional waterwarp.
  4. If bloom is enabled, add bloom on top using an additive pass.
- DOF params are stored in the uniform block:
  - `u_dof_params.x/y` hold focus/range overrides.
  - `u_dof_params.z/w` cache the 3D projection depth terms.
  - `u_vieworg.w` stores the transition strength.

## Notes
- Bloom and waterwarp remain supported; bloom is composited after DOF when both are active.
- The DOF shader linearizes depth from the projection matrix and uses a smoothstep falloff.
