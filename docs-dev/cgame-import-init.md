# Cgame Import Init Crash Fix

## Problem
During early client UI initialization, `CG_FillImports` built a `cgame_import_t`
struct using `CL_FRAMETIME` (which maps to `cl.frametime.time`). At this point
the client has not yet received server timing data, so `cl.frametime.time` is
still zero, causing an integer divide-by-zero when computing `tick_rate`.

## Fix
`CG_FillImports` now clamps the initial frame time to `BASE_FRAMETIME` when
`CL_FRAMETIME` is zero. The import values are derived from the safe
`frame_time_ms` value, which keeps the initial UI boot path stable until the
client receives a real server frame time.

## Runtime Behavior
Once the client connects and updates `cl.frametime`, the cgame interface uses
the updated values for frame timing in normal execution. The fallback only
applies during early UI startup before any server timing is available.
