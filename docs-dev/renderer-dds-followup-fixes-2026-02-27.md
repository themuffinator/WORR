# Renderer DDS Follow-up Fixes (2026-02-27)

## Summary
- Fixed two DDS integration edge cases discovered after the initial `.dds` rollout:
  - alpha-only DDS data incorrectly treated as fully opaque
  - `load_img(...)` in the RTX refresh path could index past `img_loaders` for unknown extensions

## Changes

### 1) Preserve alpha-only DDS semantics
- File: `src/renderer/dds.c`
- Function: `R_DecodeDDS(...)`
- Fix:
  - Kept alpha mask data for `DDS_ALPHA` (alpha-only) payloads even when `DDS_ALPHAPIXELS` is not set.
  - This prevents alpha-only images from being forced to opaque alpha during decode.

### 2) Safe unknown-extension handling in RTX image loader
- File: `src/rend_rtx/refresh/images.c`
- Function: `load_img(const char *name, image_t *image)`
- Fix:
  - Added explicit `fmt == IM_MAX` handling.
  - Unknown extensions now route through `try_other_formats(IM_MAX, ...)` and return `Q_ERR_INVALID_PATH` when nothing is found.
  - Avoids out-of-bounds access on `img_loaders[fmt]` for unrecognized extensions.

## Result
- DDS alpha-only assets now decode with correct transparency behavior.
- RTX refresh image loading is robust against invalid/unknown filename extensions.
