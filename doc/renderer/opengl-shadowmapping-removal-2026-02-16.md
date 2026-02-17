# OpenGL Shadowmapping Removal (2026-02-16)

## Summary
This change removes the OpenGL shadowmapping implementation from `rend_gl` and strips its debug/telemetry hooks from `cgame`.

The old shadowmap pipeline (dynamic-light shadow atlas, sun/cascade path, shadowmap-specific cvars, and shadowmap shader plumbing) no longer exists in the OpenGL renderer.

`gl_shadows` (projected/ground model shadows) is still present and unchanged.

## Goals
- Remove all OpenGL shadowmapping runtime paths.
- Remove shadowmap-specific renderer state, cvars, uniforms, and texture units.
- Remove shadowmap debug integration in `cgame`.
- Keep non-shadowmap OpenGL rendering behavior intact.

## Implementation Details

### 1. Removed shadowmap runtime from `rend_gl/main.c`
- Deleted the full shadowmap subsystem block:
  - shadowmap support/limits checks
  - atlas/cache/candidate selection logic
  - sun/cascade setup and rendering
  - shadow debug overlays, counters, and dump command paths
- Removed frame-loop hooks for shadow passes and shadow debug updates.
- Removed shadowmap-related cvar registration and command registration (`gl_shadow_dump`).
- Removed remaining atlas debug teardown call from renderer shutdown.

### 2. Removed shadowmap texture/FBO backend from `rend_gl/texture.c`
- Deleted shadowmap/sun-shadowmap texture/FBO init and shutdown code.
- Deleted shadowmap format probing and related helper functions.
- Removed `GL_ShutdownShadowmaps()` call from image shutdown.
- Reintroduced `GL_InitFramebuffers(...)` as an explicit wrapper to preserve postfx framebuffer initialization after shadowmap-block removal.

### 3. Removed shadowmap shader plumbing from `rend_gl/shader.c`
- Simplified dynamic-light shader generation:
  - removed shadowmap/cascade sampling code
  - kept unshadowed dynamic light accumulation
- Removed shadowmap-specific uniform members from generated uniform block.
- Removed shadowmap fields from generated dynamic-light struct.
- Removed shadowmap sampler declarations and texture-unit bindings.
- Removed shadowmap-driven setup in `shader_load_lights()` and `shader_setup_3d()`.
- Simplified per-pixel-lighting enable check to `gl_per_pixel_lighting` only.

### 4. Removed shadow-pass branches in world/mesh/tess draws
- `rend_gl/world.c`:
  - removed shadow-only leaf marking logic and branch paths
  - normalized world/bmodel flow to non-shadowpass path
- `rend_gl/mesh.c`:
  - removed shadow-pass rendering branch and associated state path
  - normalized mesh setup and draw flow to standard path
- `rend_gl/tess.c`:
  - removed shadow-pass draw-face path
  - removed shadowmap conditionals from `GL_Flush3D()` state/array setup

### 5. Removed shadowmap symbols from renderer API surface (`rend_gl/gl.h`)
- Removed shadowmap/cascade constants and state fields from `glStatic_t` and `glRefdef_t`.
- Removed shadowmap-related cvar extern declarations.
- Removed shadowmap texture units from `glTmu_t`.
- Removed shadowmap/cascade fields from `glUniformBlock_t` and `glDlight_t`.
- Removed shadowmap API declarations (`GL_InitShadowmaps`, `GL_ShutdownShadowmaps`, etc.).
- Removed `GLS_SHADOWMAP` from shader flag definitions/masks.

### 6. Removed stale split shader source files
- Deleted unused files carrying shadowmap code:
  - `src/rend_gl/sp1.c`
  - `src/rend_gl/sp2.c`
  - `src/rend_gl/sp3a.c`
  - `src/rend_gl/sp3b.c`

### 7. Removed OpenGL shadow debug HUD path in `cgame`
- `src/game/cgame/cg_draw.cpp`:
  - removed `gl_shadow_*` cvar handles
  - removed GL shadow debug text block from FPS/debug overlay logic
  - removed GL shadow debug cvar registration in `CG_InitScreen()`

## Validation
- Build validation performed:
  - `meson compile -C builddir`
  - Result: success

## Behavioral Impact
- OpenGL no longer renders shadowmaps for dynamic lights or sun/cascades.
- OpenGL shadowmap-specific cvars/commands/debug counters are removed from runtime usage.
- Existing non-shadowmap rendering and postfx paths continue to compile and run.
- Ground/model shadow behavior controlled by `gl_shadows` remains.
