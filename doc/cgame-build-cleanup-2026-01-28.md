# CGame sv_paused fallback and warning cleanup (2026-01-28)

## Summary
Resolved cgame build errors and eliminated warnings by fixing `sv_paused` access and suppressing the q2proto include diagnostic in cgame headers.

## Details
- Added a safe `CG_SvPausedVar()` helper in `src/game/cgame/cg_entity_local.h` and used it where heatbeam particle effects depend on `sv_paused`.
- Ensured `cg_sv_paused` is initialized in `src/game/cgame/cg_entity_api.cpp` so cgame can read pause state even if the import pointer is not ready.
- Suppressed the clang `-Wmicrosoft-include` warning around the `client_state.h` include in `inc/client/cgame_entity.h`.
- Resolved `Com_*` macro redefinition warnings by undefining before redefinition in `src/game/cgame/cg_entity_local.h`.

## Files touched
- `src/game/cgame/cg_entity_api.cpp`
- `src/game/cgame/cg_entity_local.h`
- `src/game/cgame/cg_tent.cpp`
- `inc/client/cgame_entity.h`

## Build result
- `ninja -C builddir` completes with no errors or warnings.
