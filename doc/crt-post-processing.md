# CRT Post-Processing Effect Recommendation

## Overview
To add a convincing cathode-ray tube presentation while staying performant, adopt the **CRT-Lottes** post-processing shader by Timothy Lottes. Originally authored for GPU-based emulation, the shader faithfully reproduces analog display characteristics using a single pass and minimal state, making it well-suited for integration into WORR's renderer pipeline.

## Visual Quality Highlights
- **Subpixel phosphor simulation:** Separates RGB components into staggered triads that emulate aperture grille CRTs, yielding crisp yet authentic color reproduction.
- **Dynamic scanlines:** Modulates scanline intensity according to input luminance, preventing dark scenes from becoming overly dim.
- **Masking & bloom controls:** Adjustable parameters expose per-platform tuning for mask strength, gamma, curvature, and bloom, enabling close matches to arcade or consumer displays.
- **Geometric distortion:** Optional barrel distortion and vignette settings mimic curved CRT glass without severe edge stretching.

## Performance Characteristics
- **Single-pass GLSL shader:** Avoids multi-pass ping-ponging, minimizing bandwidth pressure and simplifying integration with the existing post-processing chain.
- **Parameter driven:** Users can disable curvature or mask emulation to further reduce cost on low-end GPUs.
- **Wide adoption:** Proven across RetroArch, ReShade, and custom engines; runs comfortably on mid-range hardware, giving confidence in real-time suitability.

## Integration Notes
1. Port the GLSL implementation from the Libretro shader pack (`crt/shaders/crt-lottes.glsl`) or use the HLSL variant (`crt-lottes.fx`) as a reference when translating to WORR's shading language.
2. Feed the shader with linear color buffers and apply gamma correction within the effect to preserve intended brightness.
3. Expose key uniforms (mask strength, scanline weight, curvature, sharpness) to configuration so players can tailor the look.
4. Consider pairing with an adjustable bloom pass to simulate phosphor persistence when bright highlights fade.

## Licensing and Provenance
- **License:** MIT (compatible with WORR's licensing).
- **Source:** Maintained within the [libretro/glsl-shaders](https://github.com/libretro/glsl-shaders/blob/master/crt/shaders/crt-lottes.glsl) repository and mirrored in the ReShade shader collection.

## Why CRT-Lottes?
CRT-Lottes strikes a balance between visual fidelity and efficiency. Heavier alternatives such as CRT-Royale deliver excellent results but rely on multi-pass processing and LUTs that are harder to maintain. Simpler scanline overlays lack the subpixel and gamma nuance needed for authentic output. CRT-Lottes occupies the sweet spot: it is tuned by default for 480p–1080p content, exposes numerous parameters for artistic control, and remains light enough for contemporary hardware to run alongside other post effects.
