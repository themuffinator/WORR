# WORR Rebrand Notes

## Scope
- Finish rebranding legacy Q2PRO user-facing strings to WORR while preserving
  protocol/format naming for compatibility.
- Update credit text to retain Q2REPRO and Q2PRO attribution with repo links.
- Add local VSCode build/debug helpers (ignored by git).

## Runtime Messaging
- Enhanced savegame detection now reports WORR support while calling out the
  Q2PRO savegame format.
- Game3 proxy init/debug logs now identify WORR protocol/extended API support
  and note Q2PRO compatibility.

## Documentation and Credits
- Man pages now include WORR/Q2REPRO lineage and Q2PRO repository links.
- The startup banner retains credits to Q2REPRO and Q2PRO with the updated
  WORR-2 repo URL.

## VSCode Helpers (Local)
- `.vscode/tasks.json` adds Meson setup/build tasks.
- `.vscode/launch.json` adds client/dedicated launch configurations.
- `.vscode/` is ignored to keep editor settings local.
