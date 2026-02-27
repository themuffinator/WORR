# Shadowmap Format Fallback

## Summary
Shadowmap initialization could trigger GL_INVALID_OPERATION on some drivers
when attaching RG16F array textures to the shadowmap FBO. That failure
left shadowmaps disabled and the GL error persisted into R_RenderFrame.

## Root Cause
Some drivers do not accept RG16F array textures as color-renderable FBO
attachments. The failure shows up as GL_INVALID_OPERATION during shadowmap
setup and shadowmaps never become active.

## Fix
- Shadowmap initialization now tries a small format ladder until the FBO
  is complete:
  1) RG16F (preferred)
  2) RGBA16F
  3) RG16
  4) RG8
  5) RGBA8
- A warning is logged when the preferred format is unavailable, and
  shadowmaps continue with the first supported alternative.

## Impact
- Prevents GL_INVALID_OPERATION during shadowmap setup on affected GPUs.
- Keeps VCM/PCF functionality intact, with reduced precision on lower
  fallback formats.

## Files
- src/rend_gl/texture.c

## Verification Notes
- Load a map with `gl_shadowmaps 1` and confirm shadows appear.
- Check the console for a single format fallback warning, if any.
