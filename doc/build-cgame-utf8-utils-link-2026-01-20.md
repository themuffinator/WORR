# cgame UTF-8 Utils Link Fix (2026-01-20)

## Issue
- `cgamex86_64.dll` failed to link with undefined symbols:
  `UTF8_CountChars`, `UTF8_OffsetForChars`, and `UTF8_EncodeCodePoint`.

## Cause
- The cgame DLL includes `src/common/field.c`, which depends on UTF-8 helper
  functions defined in `src/common/utils.c`.
- Pulling the full `utils.c` into the cgame build introduced extra engine-side
  globals (like `com_framenum`) and duplicate UI bridge symbols.

## Fix
- Extracted the UTF-8 helpers into `src/common/utf8.c`.
- Linked `src/common/utf8.c` into the cgame DLL so `field.c` resolves UTF-8
  symbols without pulling in the rest of `utils.c`.

## Result
- The cgame DLL links successfully without UTF-8 symbol errors.
