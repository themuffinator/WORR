# Cgame Load Path Priority

## Change
Cgame DLL lookup now prefers the install base directory (`sys_libdir`) before the
home/appdata directory (`sys_homedir`). This matches the sgame deployment layout
where both binaries sit in the same `baseq2` under the game install.

## Rationale
The client was repeatedly probing the appdata path first, which produced noisy
errors when the cgame DLL only exists in the install directory. Prioritizing the
install location avoids those failures while retaining the homedir fallback.
