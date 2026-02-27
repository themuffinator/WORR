# Cgame Wheel C++ Compatibility Fixes

## Summary
- The cgame wheel now uses local helpers for rounding, formatting, and vec2 math
  so it builds cleanly in the C++ game module without pulling in engine-only
  C macros.
- cgame export hooks now use `button_t *` for `Wheel_ApplyButtons` and
  `WeaponBar_Input` to match the engine's `usercmd_t::buttons` type.
- `vec2_t` gained an `operator[]` in `bgame` so array-style access works in
  C++ game code.

## Details
- `src/game/cgame/cg_wheel.cpp` defines local helpers (`CG_Rint`,
  `CG_Snprintf`, `CG_Scnprintf`) and vec2 utilities (`Vector2*`, `Dot2Product`)
  to avoid including `shared/shared.h`, which conflicts with C++ enum names.
- `inc/shared/game.h` and `src/game/bgame/game.h` update the cgame exports to
  use `button_t *` for command button mutation.
- `src/game/bgame/game.h` adds `operator[]` to `vec2_t` for direct element
  access in C++.
