## Crash context

Changing display settings and exiting the menu triggered a total renderer
restart. During `CL_InitRenderer`, `vid->set_mode()` called `Win_SetMode`,
which called `SCR_ModeChanged`, which called `Con_CheckResize`. The console
still held a pointer to its previous TTF font after `Font_Shutdown`, so
`Con_FontCharWidth` dereferenced freed font memory inside
`Font_MeasureString`, causing the access violation seen in the dump.

## Fix

- Added `Con_RendererShutdown` to explicitly clear console font pointers and
  renderer handles before `Font_Shutdown`.
- Wired the new shutdown step into `CL_ShutdownRenderer` so a total restart
  leaves the console in a safe state until `Con_RegisterMedia` reloads fonts.

## Files touched

- `src/client/console.cpp`
- `src/client/client.h`
- `src/client/renderer.cpp`
