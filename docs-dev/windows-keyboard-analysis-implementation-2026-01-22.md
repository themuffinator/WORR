# Windows Keyboard Analysis Implementation (2026-01-22)

## Summary
- Completed the remaining Windows keyboard layout fixes from the analysis document.
- Added AltGr suppression, dead key handling, layout caching, and input state resets.
- Implemented IME result string input for CJK text entry.

## Key Changes
- Dead keys from `MapVirtualKeyEx` now return 0 in `win_translate_char_key` so composition is handled by `WM_CHAR`.
- AltGr detection suppresses synthetic Ctrl events by deferring left Ctrl and dropping it when Right Alt is pressed.
- Keyboard layout is cached in `win_state_t` and updated on `WM_INPUTLANGCHANGE` along with the ANSI codepage.
- UTF-8 and surrogate state is cleared on focus loss to avoid stale byte sequences.
- Console toggle suppression uses scancode 0x29 and ignores `WM_CHAR`/`WM_DEADCHAR` for that key.
- Added IME handling: `WM_IME_COMPOSITION` reads `GCS_RESULTSTR` and emits Unicode codepoints; `WM_IME_CHAR` is ignored while IME results are consumed.
- Added a debug-only warning when `MapVirtualKeyEx` returns characters outside the bindable ASCII range.

## Files Updated
- `src/windows/client.c`: input state, AltGr suppression, IME handling, layout caching, dead key handling.
- `src/windows/client.h`: cached keyboard layout field in `win_state_t`.
- `src/windows/meson.build`: link `imm32` for IME APIs.
- `src/client/keys.cpp`: clear the console suppression marker after the next char event.
