# GLSL Shader Buffer Overflow Fix - 2026-01-25

## Summary
- Fixed a GLSL buffer overflow during shader generation that triggered `SZ_GetSpace: GLSL: overflow without allowoverflow set` at level load.

## Root Cause
- The generated GLSL source grew past the static `MAX_SHADER_CHARS` limit in `src/rend_gl/shader.c` after adding the new shadowmapping features (additional CSM/PCSS/VSM/EVSM paths).

## Fix
- Increased the shader source buffer to `65536` bytes to accommodate the expanded shader text.

## Files Updated
- `src/rend_gl/shader.c`

## Validation Notes
- Load a map with shadowmapping enabled and verify shader compilation completes without a GLSL overflow.
