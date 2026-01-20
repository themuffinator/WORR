# CRT Postprocess (GL renderer)

## Overview
- Added a CRT postprocess pass to the GL renderer, composed after bloom/DOF/waterwarp/rescale.
- Added a dedicated full-screen postprocess target (FBO_CRT / TEXNUM_PP_CRT) so the CRT pass can consume the fully composed image.

## CVars
- r_crtmode (CVAR_ARCHIVE, default 0)
- r_crt_brightboost (CVAR_SERVERINFO, default 1.5)
- r_crt_hardPix (CVAR_SERVERINFO, default -8.0)
- r_crt_hardScan (CVAR_SERVERINFO, default -8.0)
- r_crt_maskDark (CVAR_SERVERINFO, default 0.5)
- r_crt_maskLight (CVAR_SERVERINFO, default 1.5)
- r_crt_scaleInLinearGamma (CVAR_ARCHIVE, default 1.0)
- r_crt_shadowMask (CVAR_ARCHIVE, default 0.0)

## Pipeline changes
- GL_BindFramebuffer treats r_crtmode as a postprocess flag and reinitializes framebuffer resources on toggle.
- Postprocess output routes into FBO_CRT when CRT is enabled, then a final GLS_CRT pass draws to the backbuffer.
- Bloom/DOF/waterwarp/rescale behavior is preserved; CRT is applied last.

## Shader/UBO details
- Added CRT uniforms to the UBO for pixel/scan softness, bright boost, gamma toggle, mask parameters, and texel sizing.
- GLS_CRT fragment shader implements Lottes-style gaussian filtering, scanline shaping, and optional shadow-mask patterns.
- Shadow mask modes: 0=off, 1=aperture grille RGB, 2=offset triad, 3=slot mask.

## Notes
- CRT pass runs only in the GL shader path; legacy GL rendering remains unchanged.
