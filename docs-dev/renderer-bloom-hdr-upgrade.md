# GL bloom + HDR postprocess upgrade

## Why
The legacy bloom path only blurred the glow buffer and then added it directly
to the scene, which limited quality and control. There was no bright-pass,
no color correction, and no HDR tonemapping control.

## What changed
- Bloom now uses a prefilter pass that downsamples the scene + glow buffer,
  applies a soft-knee bright-pass, and then runs the Gaussian blur chain.
- Final composition is handled by a dedicated postprocess pass that mixes the
  base scene and bloom, applies scene/bloom saturation, and runs color
  correction and HDR tonemapping when enabled.
- Added an HDR-capable postprocess path using RGBA16F buffers with a fallback
  to LDR if the framebuffer is incomplete.
- Added a new full-resolution postprocess texture/FBO for DOF output, so DOF
  and bloom can be composed before the final color pipeline.

## New controls
- Bloom shaping:
  - `gl_bloom`: blur iteration count (0 disables bloom).
  - `gl_bloom_threshold`: bright-pass threshold.
  - `gl_bloom_knee`: soft-knee width.
  - `gl_bloom_intensity`: bloom contribution scale.
  - `gl_bloom_scene_saturation`: saturation applied to the base scene before
    bloom mix.
  - `gl_bloom_saturation`: saturation applied to the bloom texture.
- Color correction:
  - `gl_color_correction`: toggles postprocess color correction.
  - `gl_color_brightness`: additive lift.
  - `gl_color_contrast`: contrast multiplier around 0.5.
  - `gl_color_saturation`: final saturation control.
  - `gl_color_tint`: tint color (supports named colors or hex, eg `#ffccaa`).
- HDR:
  - `gl_hdr`: toggles HDR postprocess buffers + tonemapping.
  - `gl_hdr_exposure`: exposure multiplier.
  - `gl_hdr_white`: white point for tonemap normalization.
  - `gl_hdr_gamma`: output gamma applied when HDR is enabled.

## Notes
- HDR uses RGBA16F postprocess textures when available; if the FBO is not
  complete, it falls back to LDR and logs a warning once.
- Tonemapping uses an ACES-style curve with exposure and white-point
  normalization, then optional color correction.
- `gl_showbloom` still visualizes the blurred bloom buffer directly for debug.

## Files
- `src/rend_gl/main.c`: postprocess pipeline rebuild, DOF output staging,
  postfx uniform updates, and new tuning cvars.
- `src/rend_gl/shader.c`: bloom prefilter, tonemap/color correction, new
  uniforms, and sampler bindings.
- `src/rend_gl/texture.c`: HDR-capable postprocess textures and the new post
  FBO setup.
- `src/rend_gl/gl.h`: postprocess state bits, uniform block fields, and
  texture/FBO definitions.
