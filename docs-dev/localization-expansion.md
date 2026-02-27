# Localization Expansion

This document captures the localization expansion work for WORR, including new
language assets, automatic language selection, and broader localization usage in
the game modules.

## Language Support

Added 16 additional languages (in addition to the 6 existing ones):
- Arabic
- Bulgarian
- Chinese (Simplified)
- Chinese (Traditional)
- Czech
- Danish
- Dutch
- Finnish
- Japanese
- Korean
- Norwegian
- Polish
- Portuguese (Brazil)
- Portuguese (Portugal)
- Swedish
- Turkish

Total supported languages: 22.

## New Localization Assets

New localization files were added under `assets/localization/` using the naming
pattern `loc_<language>.txt`. These new files currently mirror the English
localization content as placeholders. They can be replaced with real translations
as they become available.

## Language Selection Behavior

- A new `loc_language` cvar (default: `auto`) selects the localization file.
- On Windows, `auto` maps to the OS locale and selects the best matching
  localization file when `loc_file` is still at its default value.
- If the detected language is unsupported or the file is missing, the system
  falls back to English.
- Explicitly setting `loc_language` forces the corresponding localization file.
- `loc_file` is still supported for manual overrides (custom paths/files).

## Basedir Localization Lookup

Localization loading now accepts paths prefixed with the base game directory
(e.g. `baseq2/localization/loc_english.txt`) by stripping the base prefix and
retrying the load from the virtual filesystem search paths.

## Game Module Localization Coverage

All `gi.Client_Print`, `gi.Broadcast_Print`, and `gi.Center_Print` calls in the
sgame module now route through localization-aware wrappers. This expands
localization support across player-facing messages without changing their
semantics, and makes them ready for key-based localization where desired.
