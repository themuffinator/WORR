# Renderer image path sanitization (2026-01-25)

## Summary
- Added renderer-side sanitization for image names that fail Quake path validation.
- Strips invalid bytes and trims trailing whitespace/control characters before extension checks and load attempts.
- Applied to both OpenGL and Vulkan image loaders to keep behavior consistent.

## Rationale
Some callers can pass image names containing hidden non-printable bytes (e.g., stray control codes). These bytes make `FS_ValidatePath` reject otherwise valid paths and can surface as fatal initialization errors when fonts or UI images are loaded. Sanitizing the path only when validation fails keeps normal paths unchanged while recovering from corrupted input.

## Implementation
- Introduced `img_path_char_ok` and `img_sanitize_path` helpers in each renderer.
- If `FS_ValidatePath(name)` returns `PATH_INVALID`, the name is re-copied with invalid characters dropped and trailing whitespace trimmed.
- If the sanitized result is empty, the loader reports `Q_ERR_INVALID_PATH` as before.

## Files updated
- `src/rend_gl/images.c`
- `src/rend_vk/refresh/images.c`
