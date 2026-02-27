# Sgame Load Order: Current Dir First

## Change
The sgame loader now tries the current working directory (the worr.exe directory
on Windows) before any other basedir search paths, matching cgame behavior.

## Lookup Order
For both `game` and `baseq2`:
1) `.` (current working directory; set to the executable directory on Windows)
2) `sys_basedir` (when set and not `.`)
3) `sys_libdir`
4) `sys_homedir` (appdata/home fallback)

## Rationale
This keeps sgame co-located with the executable for local builds and reduces
noisy access failures when the DLL only exists alongside `worr.exe`.
