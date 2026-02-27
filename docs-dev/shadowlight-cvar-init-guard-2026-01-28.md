# Shadowlight cvar init guard

Date: 2026-01-28

## Summary
- Prevented null cvar access for `cl_shadowlights` in client rendering paths.

## Details
- `src/client/main.cpp`
  - Ensures `cl_shadowlights` is registered during client cvar init.
- `src/client/view.cpp` and `src/client/entities.cpp`
  - Guarded `cl_shadowlights` pointer before dereferencing to avoid startup crashes.
