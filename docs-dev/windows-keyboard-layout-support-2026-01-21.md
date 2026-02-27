# Windows Keyboard Layout Support (2026-01-21)

## Summary
- Replaced the fixed US scancode mapping with layout-aware virtual-key translation for Windows key events.
- Split text input from key events on Windows by consuming WM_CHAR/WM_SYSCHAR and routing characters through a new Key_CharEvent path.
- Added a client-side switch to disable Key_Event character generation when the platform supplies text input.

## Implementation Details
- Key event translation now prioritizes virtual-key handling for non-character keys (function keys, navigation cluster, keypad, modifiers) and uses MapVirtualKeyEx for layout-aware ASCII characters.
- If a key does not map cleanly to ASCII (e.g., non-ASCII layout characters), the handler falls back to the legacy scancode table to keep bindings usable.
- WM_CHAR/WM_SYSCHAR now feed Key_CharEvent, with UTF-8 transliteration for non-ASCII characters to match the engine’s ASCII input limits.
- Key_Event’s internal character generation is guarded behind a new Key_SetCharEvents toggle; Windows disables it to avoid duplicate text input.

## Notes
- Text input remains limited to ASCII 32-126; non-ASCII characters are transliterated to the closest Quake encoding or ignored when no mapping exists.
- Key bindings still use ASCII key codes and K_* key identifiers, so characters outside ASCII continue to fall back to US scancode mappings.
