# Filesystem Current-Dir Priority Fix (2026-02-19)

## Summary
Filesystem path resolution now uses this priority order:

1. current path (`basedir`, local `.install`)
2. detected game install path (for example Steam rerelease)
3. home path (Saved Games)

This resolves localization mismatches during local `.install` launches where newer WORR localization keys were being treated as missing.
It also restores map fallback: if maps are absent from `.install/baseq2/maps`, they are loaded from the detected install path.

## Problem
When launching from `.install` on Windows, filesystem initialization could still auto-detect and switch `basedir` to an external Quake II rerelease installation (for example, Steam).  
That caused localization files from the external install to load instead of local staged files.

Symptoms included raw localization tokens being printed, such as:

- `cl_requesting_connection`
- `cl_connected_to_server`
- `g_sgame_auto_8d9eaaf535d0`

## Root Cause
1. `FS_FindBaseDir()` was overriding `basedir` to the detected external install in some runs.
2. A previous fix removed that override but did not retain detected install content as a fallback search root.
3. With local `.install` lacking stock maps, search could miss external map files entirely.

## Implementation
### 1) Keep local `basedir` as primary, detect install as secondary
Updated `FS_FindBaseDir()` in `src/common/files.c` to:

- keep `sys_basedir` unchanged (current path stays primary),
- detect external install path via `gamepath_funcs`,
- store that detected path in `fs_detected_basepath` for fallback search use.

### 2) Enforce explicit priority: current > install > home
Updated path setup ordering in `src/common/files.c`:

- `setup_base_paths()`: add home first, then detected install, then current basedir.
- `setup_game_paths()`: add home first, then detected install, then current basedir.

Because search paths are prepended, this produces:

- current (`basedir`) first,
- install path second,
- home path third.

### 3) Launch config hardening
Updated `.vscode/launch.json` to pass:

- `+set basedir ${workspaceFolder}\\.install`

for all local launch profiles to keep debug runs explicit and deterministic.

## Validation
After rebuild/stage, running:

```powershell
.install\worr.ded.exe +set logfile 0 +set developer 1 +quit
```

now reports search path priority with local base first:

1. `./baseq2/worr-assets.pkz`
2. `./baseq2`
3. `...Steam.../rerelease/baseq2/...`
4. `Saved Games .../baseq2`

and loads local localization content (`Loaded 2851 localization strings` in this test build).
Also validated map fallback by running `+map q2dm1`; map loads successfully from detected install content when not present locally.

## Files Changed
- `src/common/files.c`
- `.vscode/launch.json`
- `doc/filesystem-currentdir-priority-2026-02-19.md`
