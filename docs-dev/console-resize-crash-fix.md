# Console Resize Crash Fix (External Renderer)

## Issue
- `Con_CheckResize` calls `R_ClampScale` during early startup.
- With external renderers enabled, `R_ClampScale` maps to `re.ClampScale`.
- Before the renderer DLL is loaded (or after a load failure), `re.ClampScale` was NULL, causing an access violation at address `0x0000000000000000`.

## Change
- Added a `R_ClampScaleStub` in the engine that:
  - Returns a clamped user scale when explicitly set.
  - Falls back to `1.0f` when auto-scale is requested.
- Initialized `re.ClampScale` to the stub at startup.
- Restored the stub in `R_UnloadExternalRenderer` after unload/failure.

## Impact
- Console resize and other early UI paths no longer crash before the renderer DLL is loaded.
- External renderer builds still use the real renderer implementation once it is available.
