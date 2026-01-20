# PostFX FBO Fallbacks and MRT Compatibility

## Summary
Some Windows 11 NVIDIA systems report `GL_FRAMEBUFFER_UNSUPPORTED` for `FBO_SCENE`,
which disables all post-processing (`framebuffer_ok(no)`). This update makes the
post-process FBO initialization more robust by:
- Explicitly setting draw buffers for `FBO_SCENE`.
- Adding fallback attempts that disable bloom MRT or DOF depth when necessary.
- Exposing the active postfx configuration in `gfxinfo`.

## Fallback Strategy
When post-processing FBO initialization fails, the renderer now retries with
progressively simpler configurations:
1. Requested settings (DOF depth + bloom MRT).
2. Bloom MRT disabled (scene-only bloom).
3. DOF disabled (depth texture removed).
4. Both bloom MRT and DOF disabled.

If a fallback succeeds, the renderer logs why:
- `PostFX: bloom MRT unsupported, using scene-only bloom.`
- `PostFX: DOF depth unsupported, disabling DOF.`

This keeps color correction, HDR tonemapping, CRT, water warp, and rescale working
even if bloom glow (MRT) or DOF depth textures are unsupported.

## Bloom MRT Behavior
Bloom glow uses MRT (second color attachment) to write glow contributions during
scene rendering. When MRT is disabled, bloom still runs using scene brightness
thresholding, but glowmap contributions are skipped.

## gfxinfo Diagnostics
`gfxinfo` now reports postfx configuration:
```
PostFX: shaders(on) hdr_active(no) framebuffer_ok(yes) fb(2560x1440) render(2560x1440) bloom_mrt(no) dof_depth(no)
```
Use this to confirm whether MRT and DOF depth textures are active.

## Notes
- Fallback is automatic; toggling `gl_bloom`, `r_dof`, or resizing will reattempt
  the preferred configuration.
- If postfx still fails, enable `gl_showerrors 1` and capture the `FBO_*` failure
  line to identify the unsupported attachment combination.
