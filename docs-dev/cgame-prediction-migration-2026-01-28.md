# Cgame prediction migration

Date: 2026-01-28

## Summary
- Client-side prediction (movement + error correction) now lives in the cgame entity module.
- Cgame entity API version bumped and extension names updated to reflect the new prediction entrypoints.
- Engine prediction entrypoints now delegate to cgame entity exports.

## Details
- inc/client/cgame_entity_ext.h
  - API bumped to v2 with new import/export extension names.
- inc/client/cgame_entity.h
  - Removed CL_PredictAngles/CL_CheckPredictionError imports.
  - Added CL_PointContents and Pmove imports.
  - Added PredictMovement and CheckPredictionError exports.
- src/client/cgame.cpp
  - cgame entity import struct now supplies CL_PointContents and a Pmove wrapper (Pmove + cl.pmp).
  - Removed prediction-related imports that are now owned by cgame.
- src/client/predict.cpp
  - CL_PredictMovement and CL_CheckPredictionError now dispatch to cgame entity exports.
  - CL_PointContents promoted to a global export for cgame usage.
- src/game/cgame/cg_predict.cpp
  - New home for prediction logic (trace/clip/point-contents wrappers, movement predict, error check).
- src/game/cgame/cg_entity_api.cpp
  - Exports PredictMovement and CheckPredictionError.
- src/game/cgame/cg_entity_local.h + src/game/cgame/cg_client_defs.h
  - Added CL_PointContents macro and prediction prototypes.
- meson.build
  - Added cg_predict.cpp to the cgame build.

## Notes
- The new v2 extension names intentionally invalidate older cgame entity builds.
- Prediction uses the engine Pmove implementation with the current cl.pmp parameters via the wrapper.
- CL_PredictAngles remains in the engine for initial predicted angles when the server omits viewangles; the prediction loop itself is cgame-owned.
