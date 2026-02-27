# Renderer Client Cvar Resolution

## Issue
- External renderer loads before client cvars are created, leaving imported cvar pointers (`cl_gunfov`, `cl_adjustfov`, `cl_gun`, `info_hand`, `cl_async`) null.
- Even with pointer guards, optimized builds can still hit null dereferences in the renderer DLL.

## Change
- Added `Cvar_VariableValue` to the renderer import API.
- Switched renderer DLL code paths to query client cvars by name using `Cvar_VariableValue`/`Cvar_VariableInteger`:
  - `setup_weaponmodel` now uses values instead of imported pointers.
  - `R_EndFrame` now uses `cl_async` via `Cvar_VariableInteger`.

## Impact
- Renderer no longer depends on client cvar pointer availability.
- External renderer startup is stable even when client cvars are initialized later.
