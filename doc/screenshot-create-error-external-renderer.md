# Screenshot Create Error With External Renderers

## Symptom
- `screenshot`/`screenshotpng` report "Couldn't create screenshot: Unspecified error"
  after at least one `quakeXXX` file already exists in `screenshots/`.

## Root Cause
- The renderer DLL used the engine's `Q_fopen` to probe for a free `quakeXXX`
  filename, then relied on `Q_ERRNO` for the error code when the open failed.
- `Q_fopen` runs inside the engine CRT, but `Q_ERRNO` reads `errno` from the
  renderer CRT. When a file already exists, the engine CRT sets `errno=EEXIST`,
  while the renderer CRT still sees `errno=0`, which maps to `Q_ERR_FAILURE`
  ("Unspecified error").
- The probe loop aborted on the first existing file instead of continuing.

## Fix
- Added a renderer-local exclusive open helper that uses the renderer CRT
  (`_open`/`open` + `fdopen`), ensuring `errno` is visible to `Q_ERRNO`.
- `create_screenshot` now uses that helper when probing auto-numbered names.

## Files Touched
- `src/rend_gl/images.c`

## Validation Notes
- Take multiple screenshots with existing `screenshots/quake000.*` files present.
- Confirm the command walks to the next free number and writes the file.
