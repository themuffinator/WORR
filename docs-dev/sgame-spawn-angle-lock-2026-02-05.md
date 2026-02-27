# Sgame Spawn Angle Lock (2026-02-05)

## Summary
- Added a short spawn-angle lock for map-defined spawn points so players stay frozen for roughly two frames and keep the spawn point orientation during the first movement frame.

## Details
- `ClientSpawn` now tags spawn-point spawns (non-landmark, non-squad, non-ghost) with a brief lock timer and the spawn-point angles.
- `ClientThink` honors the lock by forcing a short `PMF_TIME_KNOCKBACK` window and overriding the incoming command angles to the stored spawn angles for the lock duration.
- The lock is only applied for active players and does not affect landmark or ghost spawns.

## Files
- `src/game/sgame/g_local.hpp`
- `src/game/sgame/gameplay/g_spawn_points.cpp`
- `src/game/sgame/client/client_session_service_impl.cpp`
