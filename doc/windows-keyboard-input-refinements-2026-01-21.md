# Windows Keyboard Input Refinements (2026-01-21)

## Summary
- Suppressed console-toggle characters so opening the console with the backtick key no longer injects a stray character into the input field.
- Corrected WM_CHAR handling under UTF-8 codepage windows by decoding UTF-8 byte sequences before transliteration, preventing multi-byte glyphs from being split into extra ASCII characters.

## Details
- Key_Event now records a one-shot suppression of the console toggle key when character events are supplied by the platform (Windows), ensuring WM_CHAR for the toggle key is ignored.
- WM_CHAR processing buffers UTF-8 lead/continuation bytes when the active ANSI code page is UTF-8 (manifested in WORR builds). Completed UTF-8 sequences are transliterated once, avoiding duplicate output such as "PSA" for the pound sign.
- Non-UTF-8 ANSI code pages still convert single bytes through MultiByteToWideChar before the normal transliteration path.

## Limitations
- Console and chat input remain ASCII-only; non-ASCII input continues to be transliterated to closest Quake-compatible characters.
