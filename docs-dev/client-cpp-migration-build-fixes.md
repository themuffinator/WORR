# Client C++ Migration Build Fixes

## Context
The client module now builds as C++, which exposed C/C++ linkage mismatches and
missing local storage for globals that used to be provided by C translation
units. This document records the fixes applied to restore successful linking.

## Changes
- Cgame UI shim globals
  - Added C-linkage definitions for `developer`, `cmd_buffer_text`,
    `cmd_buffer`, `cmd_current`, `cmd_opt*`, `com_eventTime`,
    `com_localTime*`, `net_from`, `r_config`, and `fs_gamedir` in
    `src/game/cgame/cg_ui_sys.cpp`.
  - These globals are required by the UI sources that are compiled into the
    cgame DLL, and are updated via the UI import callbacks.
- C/C++ linkage alignment for client references
  - Declared `SCR_FadeAlpha` and `SCR_ParseColor` with C linkage in
    `src/client/cgame_classic.cpp` to match the C-linkage definitions in
    `src/client/screen.cpp`.
  - Declared `SCR_NotifyMouseEvent` with C linkage in
    `src/client/ui_bridge.cpp` to match the C-linkage definition.
  - Declared `nulltexinfo` with C linkage in `src/client/tent.cpp` to match
    the definition in `src/common/cmodel.c`.
  - Declared all platform `vid_*` drivers with C linkage in
    `src/client/renderer.cpp` to match their C definitions.
  - Declared `snddma_dsound` and `snddma_wave` with C linkage in
    `src/client/sound/dma.cpp` to match their C definitions.
- FFmpeg headers in C++ translation units
  - Wrapped FFmpeg includes in `extern "C"` blocks in
    `src/client/cin.cpp`, `src/client/sound/mem.cpp`, and
    `src/client/sound/ogg.cpp` so the C symbols link correctly.

## Build Outcome
- `worr.exe` and `cgamex86_64.dll` link successfully.
- Known warnings remain (unchanged from prior builds):
  - C99 designator warning in `src/client/tent.cpp`.
  - Microsoft `goto` warnings in `src/client/sound/mem.cpp`.
