# CGame UI Font Size Imports (2026-01-22)

## Summary
- Added size-aware font draw/measure/line-height hooks to the cgame UI import table so UI code in the cgame module can request the size-6 list font through the engine.
- Added cgame-side forwarding wrappers that fall back to the base UI font calls when sized hooks are unavailable.
- Wired the client-side import table to expose the new hooks, resolving the cgame DLL link errors from the list font changes.

## Files Touched
- inc/client/cgame_ui.h
- src/client/cgame.cpp
- src/game/cgame/cg_ui_sys.cpp
