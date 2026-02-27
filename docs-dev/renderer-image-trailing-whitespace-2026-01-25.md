# Renderer image path trailing whitespace fix (2026-01-25)

## Summary
- Trim trailing ASCII whitespace/control characters from image names before extension checks or disk lookup.
- Prevents valid assets like `pics/conchars.pcx` from being rejected as `Invalid quake path` when callers supply names with hidden trailing spaces.
- Applies to both OpenGL and Vulkan image loaders for consistent behavior.

## Details
The renderer image loaders now sanitize incoming image names by trimming trailing characters `<= ' '` and rejecting empty results before extension validation and load attempts. This avoids mis-detecting file extensions (for example, `pcx `) or treating a valid Quake path as invalid when scripts or config values accidentally include trailing whitespace.

## Files updated
- `src/rend_gl/images.c`
- `src/rend_vk/refresh/images.c`
