# Renderer Weapon Model Cvar Guard

## Issue
- The external renderer DLL dereferenced client cvar pointers (`cl_adjustfov`, `cl_gunfov`, `cl_gun`, `info_hand`) during `setup_weaponmodel`.
- These pointers can be null before client cvars are initialized or if the renderer loads early.
- The result was an access violation in `worr_opengl_x86_64.dll` when reading `cl_adjustfov`.

## Change
- Added null checks around the weapon model FOV and handedness logic in `setup_weaponmodel`.
- When cvars are missing, the renderer falls back to default behavior (no gun FOV override, no mirroring).

## Impact
- Prevents external renderer crashes during early startup or reloads.
- Keeps weapon model rendering behavior unchanged once cvars are available.
