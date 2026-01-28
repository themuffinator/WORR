# Cgame entity/effect migration enforcement

Date: 2026-01-28

## Summary
- Removed engine-side fallbacks for entity, effect, and temp-entity handling.
- Client entrypoints now require the cgame entity extension once the module is loaded.
- Engine now errors if the cgame entity extension is missing during cgame load.

## Details
- src/client/cgame.cpp
  - CG_Load now requires a valid cgame entity extension and initializes effects/brightskins/tents unconditionally after load.
- src/client/entities.cpp
  - CL_DeltaFrame, CL_CalcViewValues, CL_AddEntities, and CL_GetEntitySoundOrigin now dispatch exclusively to cgame entity exports.
  - CL_CheckEntityPresent now dispatches exclusively to cgame entity export.
  - Init/migrate/forced-model entrypoints are no-ops before cgame is loaded and error if the expected cgame export is missing.
- src/client/effects.cpp
  - Lightstyle and muzzleflash parsing now require cgame entity exports.
  - Init/Clear effects are no-ops before cgame load and error if the exports are missing.
- src/client/tent.cpp
  - Temp-entity parsing and help-path are delegated exclusively to cgame entity exports.
  - TEnt init/clear and registration are no-ops before cgame load and error if exports are missing.

## Notes
- Legacy engine implementations remain in the codebase but are no longer used by runtime entrypoints.
- This aligns with the updated rule that compatibility is not required, shifting responsibility for entity/effect handling fully to cgame.
