# External Renderer Build Fixes

## Windows OpenGL Headers
- The external renderer uses `src/renderer/qgl.h`, which includes `GL/glext.h`.
- On Windows, the engine target already pulled `khr-headers`, but the external renderer target did not, so the DLL build failed with missing OpenGL headers.

### Change
- Promote the `khr-headers` dependency to a shared list for Windows builds.
- Apply that shared dependency list to both the client and external renderer targets.

### Impact
- External renderer builds (`-Dexternal-renderers=true`) now find `GL/glext.h` without relying on SDK headers.

## `va` Macro Collision
- `inc/renderer/renderer_api.h` defines `va` as an import macro for external renderer builds.
- `GL_BindArrays` used a parameter named `va`, which expanded to `ri.va` and broke compilation.

### Change
- Renamed the parameter to `va_type` to avoid macro expansion.

### Impact
- External renderer builds no longer fail on `GL_BindArrays` declaration/definition parsing.

## Debug Draw Export
- Added debug draw function pointers to the renderer export interface.
- Engine-side wrappers now forward debug draw calls to the external renderer when enabled.

### Impact
- Navigation and game debug rendering continue to work with external renderers.
- Nav link debug calculations use client view data (`cl.refdef`/`cl.v_forward`) instead of renderer globals.

## External Renderer Imports
- The renderer DLL referenced engine-only globals and functions (cmd options, palette normals, client cvars, FOV helper, hash/JSON helpers).

### Change
- Expanded `renderer_import_t` to carry `cmd_optind`, `bytedirs`, `cl_async`, `cl_gunfov`, `cl_adjustfov`, `cl_gun`, `info_hand`,
  plus `V_CalcFov`, `mdfour_*`, and `jsmn_*`.
- Populated the new imports in `src/client/renderer.c`.

### Impact
- External renderer linking no longer depends on engine symbols.

## Renderer API Include Order
- Import macros in `renderer_api.h` can rewrite common header declarations if included too early.

### Change
- Moved the `renderer/renderer_api.h` include in `src/renderer/images.c` after common headers.

### Impact
- Prevents macro expansion in `common/common.h` while still mapping imports for the renderer DLL.

## Palette Table Export
- The HUD POI color lookup used `d_8to24table`, which lives in the renderer and is absent in external builds.

### Change
- Added `PaletteTable` to `renderer_export_t` and wired it to `d_8to24table`.
- Switched the client POI rendering path to use the export when external renderers are enabled.

### Impact
- POI colors render correctly without linking to renderer globals.

## Renderer Cvar Cheat Flags
- The client toggled cheat flags on renderer cvars via direct renderer globals.

### Change
- Use `Cvar_FindVar` for `gl_modulate_world`, `gl_modulate_entities`, and `gl_brightness` when external renderers are enabled.

### Impact
- Removes link-time coupling between the engine and renderer cvar globals.

## R_DrawString Inline Placement
- `R_DrawString` was defined before the external renderer macro block, so engine builds still referenced `R_DrawStringStretch`.

### Change
- Moved the inline helper below the macro block.

### Impact
- Engine builds now call `re.DrawStringStretch` when external renderers are enabled.

## Debug Timing Source
- Renderer debug primitives used `sv` timing, which is not present in the DLL.

### Change
- Use `com_eventTime` when building the renderer DLL.

### Impact
- Debug primitives expire correctly without server globals.
