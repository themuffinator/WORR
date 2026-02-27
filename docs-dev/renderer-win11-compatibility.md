# Windows 11 GPU Preference and PostFX Diagnostics

## Overview
This update adds explicit hybrid-GPU preference exports for Windows builds and improves framebuffer
diagnostics for post-processing. The goal is to default to the high-performance GPU where possible
and to make framebuffer initialization failures actionable when post effects are disabled.

## Hybrid GPU Preference (Windows)
`src/windows/system.c` now exports the standard driver hints:
- `NvOptimusEnablement = 0x00000001`
- `AmdPowerXpressRequestHighPerformance = 1`

These exports are recognized by NVIDIA Optimus and AMD PowerXpress to prefer the discrete GPU
for the process by default. Windows Graphics Settings can still override the preference per app.

## PostFX Framebuffer Diagnostics
`src/rend_gl/texture.c` now prints detailed framebuffer status on failure:
- Status code and string (e.g., `GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT`)
- Attachment size used for the FBO
- PostFX color format/type/internal format used for the allocation

This shows up when `gl_showerrors` is enabled (default is `1`). Example:
```
FBO_SCENE framebuffer status 0x8CD6 (GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT) size 2560x1440 fmt 0x1908 type 0x1401 internal 0x8058
```

If you see `PostFX: ... framebuffer_ok(no)` in `gfxinfo`, use the above line to identify whether
the failure is due to size, format, or attachment completeness.

## gfxinfo PostFX Size Output
`gfxinfo` now prints framebuffer and render sizes in the PostFX line:
```
PostFX: shaders(on) hdr_active(no) framebuffer_ok(no) fb(0x0) render(2560x1440)
```
If `fb(0x0)` is reported, the post-process FBOs have not initialized or were skipped due to
an earlier failure.
