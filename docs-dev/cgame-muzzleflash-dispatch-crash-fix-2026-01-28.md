# Muzzleflash cgame dispatch crash fix

Date: 2026-01-28

## Summary
- Prevented engine-side muzzleflash code from running after cgame dispatch, fixing a null cvar access crash.

## Details
- src/client/effects.cpp
  - CL_MuzzleFlash and CL_MuzzleFlash2 now return immediately after invoking cgame entity exports, ensuring engine fallback logic does not execute when cgame is authoritative.

## Notes
- This aligns with the cgame-only entity/effect migration: engine muzzleflash logic is retained but no longer executed.
