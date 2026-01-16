# Cgame Load Order: Current Dir First

## Change
The cgame loader now tries the current working directory (the worr.exe directory
on Windows) before any other basedir search paths.

## Lookup Order
1) `.` (current working directory; set to the executable directory on Windows)
2) `sys_basedir` (when set and not `.`)
3) `sys_libdir`
4) `sys_homedir` (appdata/home fallback)

## Rationale
When developing or running a custom build, the cgame DLL often lives alongside
`worr.exe` instead of the Steam install or appdata paths. Prioritizing the
current directory removes noisy "Can't access" messages and makes the engine
load the cgame DLL from the same location as sgame by default.
