# Windows Unicode Input (2026-01-22)

## Goal
- Accept Unicode codepoints from WM_CHAR so non-US layouts can type native characters
  (for example U+00A3 on UK keyboards).

## Changes
- Windows WM_CHAR handling now decodes UTF-8/ACP input into Unicode codepoints and
  forwards them directly to `Key_CharEvent`.
- `Key_CharEvent` accepts Unicode codepoints (filters control ranges and surrogates).
- Menu input validation treats non-ASCII codepoints as printable for text fields.
- Input field rendering in the cgame UI and chat HUD uses UTF-8-aware cursor and
  scroll offsets so cursor placement stays aligned with multibyte text.

## Notes
- Console toggle suppression still skips the backtick character event on Windows.
- Clipboard paste remains transliterated to Quake encoding (unchanged).
