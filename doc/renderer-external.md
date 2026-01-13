# External Renderer and Renderer Rename Notes

## Overview
- Renamed the renderer subsystem from "refresh" to "renderer" (paths, entry points, and user-facing command names).
- Added optional external renderer builds via Meson `external-renderers` to produce a standalone renderer library.
- Rebranded product and application identifiers to WORR for executable/config/menu naming.

## External Renderer Build
- Meson option: `-Dexternal-renderers=true`.
- Output name format: `<product>_<graphics_api>_<cpu><libsuffix>`.
- Example (Windows x86_64): `worr_opengl_x86_64.dll`.

## Runtime Selection and Loading
- New cvar: `r_renderer` (default `opengl`) chooses the renderer library name.
- Alias: `gl` resolves to `opengl` for compatibility.
- Load order:
  1) `sys_libdir` (if set)
  2) `sys_basedir`
  3) platform default library search path

## External Renderer API
- Export entry point: `Renderer_GetAPI`.
- Engine provides `renderer_import_t` (core services, math, parsing, filesystem, sizebuf, and globals).
- Renderer exports via `renderer_export_t` now include a `Config` pointer for `r_config` access.
- Renderer-side builds define `RENDERER_DLL` and include `renderer/renderer_api.h` for import shims.

## Renderer Rename Notes
- Subsystem paths are now `src/renderer/` and `inc/renderer/`.
- Refresh-specific names were updated to renderer-oriented naming (for example, `timerenderer`).

## WORR Branding
- Product/app identifiers now use `WORR` and `worr` for names like `worrconfig.cfg` and `worr.menu`.
