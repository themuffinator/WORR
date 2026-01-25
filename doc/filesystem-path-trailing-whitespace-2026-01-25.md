# Filesystem path trailing whitespace trim (2026-01-25)

## Summary
- Trim trailing ASCII whitespace/control characters when validating Quake paths.
- Prevents false `Invalid quake path` errors when a path token carries CR/LF or other trailing whitespace from configs or scripts.

## Problem
- Valid asset paths (example: `pics/conchars.pcx`) could be rejected when the source string included trailing CR/LF or other control characters. This results in `Q_ERR_INVALID_PATH` even though the underlying path is correct.

## Resolution
- `FS_ValidatePath` now trims trailing bytes `<= ' '` before validating characters.
- Paths with only whitespace still fail validation; mixed-case detection remains unchanged.

## Compatibility
- Quake path validation remains strict for invalid characters.
- Only trailing whitespace/control characters are ignored, which is safe for legacy assets and demos.

## Files
- `src/common/files.c`
