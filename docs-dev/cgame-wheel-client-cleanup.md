# Cgame Wheel Client Cleanup

## Summary
- Client-side wheel and weapon bar state has been removed from engine headers.
- Wheel and weapon bar assets/state now live entirely in cgame.
- The engine keeps a single hook (`CL_Wheel_TimeScale`) for global time scaling.

## What Changed
- `src/client/client.h` no longer defines `cl_wheel_*` types or embeds wheel/weapon bar
  state in `client_state_t`.
- `cl_scr_t` no longer stores wheel/weapon bar art handles. Those assets are now
  registered and cached by cgame in `src/game/cgame/cg_wheel.cpp`.
- Wheel/weapon bar entry points (`CL_Wheel_*`, `CL_WeaponBar_*`) were removed from
  the engine-facing header, leaving only `CL_Wheel_TimeScale`.

## Ownership Now
- Wheel/weapon bar state, input, and draw live in cgame (`src/game/cgame/cg_wheel.cpp`)
  and are driven by cgame exports from the engine.
- The engine only queries `CL_Wheel_TimeScale` for pacing in `src/common/common.c`
  and `src/client/sound/al.c`.

## Build Notes
- The engine no longer compiles the legacy `src/client/wheel.c` or
  `src/client/weapon_bar.c` sources in `meson.build`.
