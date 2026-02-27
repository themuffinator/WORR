# Server Game DLL: Sgame Only

## Change
The legacy `gamex86_64.dll` is no longer built or loaded. The engine now uses
`sgamex86_64.dll` exclusively for server game code.

## Build Impact
- Removed the top-level Meson target that produced `gamex86_64.dll` from
  legacy `src/game/` sources (now archived under `src/legacy/game`).
- Meson produces `sgamex86_64.dll` from `src/game/sgame` (shared code in
  `src/game/bgame`) and copies it into `baseq2`.

## Runtime Impact
- `LoadSGameLibrary` no longer falls back to `game` if `sgame` is missing.
- This prevents repeated attempts to load the legacy game DLL and enforces the
  new sgame-only deployment layout.
