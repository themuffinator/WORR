# Build Linkage Fix: Cvar_Clamp* in cgame (2026-01-22)

## Summary
- Resolved the `cgamex86_64.dll` link failure for `Cvar_ClampInteger` and
  `Cvar_ClampValue` by aligning linkage between declarations and definitions.

## Root Cause
- `src/game/cgame/cg_ui_sys.cpp` defines `Cvar_ClampInteger` and
  `Cvar_ClampValue` with C linkage (via prior `extern "C"` declarations in
  included headers), producing unmangled C symbols.
- `src/game/cgame/cg_draw.cpp` declared those functions with C++ linkage,
  producing mangled C++ symbol references.
- The mismatch caused the linker to fail with undefined references to the
  mangled C++ symbols.

## Fix
- Updated `src/game/cgame/cg_draw.cpp` to declare `Cvar_ClampInteger` and
  `Cvar_ClampValue` inside an `extern "C"` block alongside other C-linked
  engine helpers, so references resolve to the C symbols exported by
  `cg_ui_sys.cpp`.

## Notes / Follow-ups
- If additional C API helpers are added for game-side code, keep their
  declarations consistent (C linkage on both sides) to avoid future
  unresolved externals.
