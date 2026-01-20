# PostFX DOF Depth Format Fallback

## Overview
Some Windows 11 NVIDIA drivers reject the post-process scene FBO when a depth
texture is attached for DOF, returning `GL_FRAMEBUFFER_UNSUPPORTED`. This update
adds a depth-format fallback ladder to keep DOF enabled when possible and to
produce clearer diagnostics when it is not.

## Depth Format Ladder
When DOF is requested, the renderer now tries multiple depth texture formats in
order until the FBO completes:
- `GL_DEPTH_COMPONENT24` (preferred on 24-bit depth devices)
- `GL_DEPTH_COMPONENT16`
- `GL_DEPTH_COMPONENT32F` (if available)
- `GL_DEPTH24_STENCIL8` (packed depth-stencil, if stencil is enabled)
- `GL_DEPTH32F_STENCIL8` (if available)

If a non-preferred depth format succeeds, a warning is printed, e.g.:
```
PostFX: depth format D24 unsupported, using D16.
```

If no depth format works, DOF is disabled and postfx continues without it:
```
PostFX: DOF depth unsupported, disabling DOF.
```

## Diagnostics
Framebuffer errors now include depth format details, for example:
```
FBO_SCENE framebuffer status 0x8CDD (GL_FRAMEBUFFER_UNSUPPORTED) size 2560x1440 fmt 0x1908 type 0x1401 internal 0x8058 depthfmt 0x1902 depthtype 0x1405 depthinternal 0x81A6
```

Use `gfxinfo` to verify whether DOF depth is active:
```
PostFX: shaders(on) hdr_active(no) framebuffer_ok(yes) fb(2560x1440) render(2560x1440) bloom_mrt(yes) dof_depth(yes)
```
