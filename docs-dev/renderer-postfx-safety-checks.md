# Renderer PostFX Safety Checks

## Summary
This update adds stricter validation and fallback logic for post-processing to
avoid driver-specific framebuffer and depth-texture failures. The goal is to
keep the renderer stable by disabling only the features that cannot be safely
initialized.

## Changes
- Framebuffer initialization now treats any GL error during setup as a hard
  failure for that attempt, ensuring unsupported formats do not slip through.
- Bloom MRT is prevalidated using GL_MAX_DRAW_BUFFERS and qglDrawBuffers.
  If unsupported, bloom falls back to scene-only without MRT.
- DOF now rejects depth-stencil depth textures by default. If depth-only
  formats are not available, DOF is disabled to prevent corruption.

## New Cvar
- `r_dof_allow_stencil` (default: 0)
  Controls whether DOF can use a depth-stencil texture. Set to 1 to force DOF
  on hardware that only exposes depth-stencil formats for depth sampling.

## Expected Impact
- On hardware that cannot create depth-only textures, DOF will be disabled by
  default, avoiding renderer glitches after driver updates.
- Bloom MRT is safely disabled when the GPU does not support multiple draw
  buffers, but bloom remains available via scene-only extraction.
