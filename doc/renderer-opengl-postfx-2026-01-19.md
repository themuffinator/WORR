# OpenGL PostFX/Bloom/HDR Upgrades (2026-01-19)

## Overview
The OpenGL post-processing pipeline now supports additional bloom quality controls, HDR auto-exposure, and professional grading tools. This update also adjusts color correction ordering and resolves bloom+postfx saturation stacking.

## Rendering Changes
- Bloom prefilter now clamps extreme luminance spikes to suppress fireflies before thresholding.
- Bloom blur passes are controlled by a dedicated iteration cvar (gl_bloom_iterations) instead of gl_bloom.
- Bloom downscale is now configurable for quality/perf tradeoffs (gl_bloom_downscale).
- Multi-level bloom is available via a mip pyramid of the blurred bloom texture, sampled in the composite pass.
- HDR postfx can use auto-exposure driven by scene mipmaps and adaptive blending.
- Color correction now applies contrast before brightness.
- Split-toning adds separate shadow/highlight tinting.
- LUT grading supports standard NxN strip LUT textures for professional color grading.

## New/Updated Cvars
Bloom
- gl_bloom (existing): enable/disable bloom.
- gl_bloom_iterations: blur passes per axis (total passes = iterations * 2). Default 1.
- gl_bloom_downscale: bloom buffer downscale factor. Default 4.
- gl_bloom_firefly: luminance clamp for prefilter firefly suppression. Default 10.0.
- gl_bloom_levels: number of mip levels to blend in bloom composite (1 = single level). Default 1.

HDR Auto-Exposure
- gl_hdr_auto_exposure: enable auto-exposure when HDR is active. Default 0.
- gl_hdr_auto_min_luma: minimum luminance clamp for exposure calculation. Default 0.05.
- gl_hdr_auto_max_luma: maximum luminance clamp for exposure calculation. Default 4.0.
- gl_hdr_auto_speed: exposure adaptation speed. Default 2.0.
- gl_hdr_exposure (existing): acts as the exposure bias when auto-exposure is enabled.

Split-Toning
- gl_color_split_shadows: shadow tint color (name/hex/rgb). Default white.
- gl_color_split_highlights: highlight tint color (name/hex/rgb). Default white.
- gl_color_split_strength: blend strength (0..1). Default 0.
- gl_color_split_balance: balance between shadows/highlights (-1..1). Default 0.

LUT Grading
- gl_color_lut: path to LUT image (pics/ prefix by default). Empty disables.
- gl_color_lut_intensity: LUT blend strength (0..1). Default 1.0.

## LUT Format
- Accepts NxN strip LUTs:
  - Horizontal strip: width = N * N, height = N.
  - Vertical strip: width = N, height = N * N.
- The LUT is sampled with trilinear interpolation across slices.
- The LUT texture is loaded with IF_NO_COLOR_ADJUST to avoid gamma/saturation preprocessing.

## Notes
- Auto-exposure is only evaluated when HDR is active and auto-exposure is enabled.
- Multi-level bloom uses a mip chain generated from the blurred bloom result; if mipmap generation is unavailable, it falls back to single-level sampling.
- Bloom downscale affects the shared blur buffers; if DOF is active, it inherits the same blur resolution.

## Testing Checklist
- Toggle gl_bloom and verify GL_BuildBloom uses gl_bloom_iterations + gl_bloom_downscale.
- Enable gl_bloom_levels > 1 and confirm multi-level sampling in bloom composite.
- Enable gl_hdr + gl_hdr_auto_exposure and verify exposure adapts to scene brightness.
- Enable split-toning and confirm shadows/highlights tinting is visible.
- Load a LUT (e.g. 16x256 or 256x16) and validate LUT intensity blending.
- Compare color correction with contrast-before-brightness ordering.
