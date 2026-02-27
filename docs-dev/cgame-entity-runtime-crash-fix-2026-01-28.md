# CGame entity runtime crash fix (2026-01-28)

## Summary
Fixed a runtime crash in the cgame temp-entity registration path where the cgame DLL invoked a null renderer function pointer during `CL_RegisterTEntModels`.

## Root cause
`cgame_entity_import_t` captured renderer function pointers from the `re` export struct at static initialization time. If the cgame DLL was loaded before the renderer export table was populated, the captured pointers (notably `re.RegisterModel` and `re.RegisterImage`) were null. Later calls from cgame (via `R_RegisterModel` in `cg_tent.cpp`) executed a null function pointer, causing an access violation.

## Fix
- Added runtime wrappers for renderer entry points in `src/client/cgame.cpp`:
  - `CG_R_RegisterModel`
  - `CG_R_RegisterImage`
  - `CG_R_SupportsPerPixelLighting`
- Updated `cgame_entity_import_t` to use these wrappers rather than the raw `re.*` pointers. This defers resolution until call time, when the renderer export table is guaranteed to be initialized.

## Files touched
- `src/client/cgame.cpp`

## Build result
- `ninja -C builddir` completes successfully.
