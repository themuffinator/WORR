# Localization fallback and sgame key coverage (2026-01-21)

## Overview
- Added a builtin English localization fallback so the client can always load `localization/loc_english.txt` even if the file is missing on disk.
- Ensured every sgame localization key referenced in code exists in the English localization file.
- Repaired missing keys in the legacy (official) language files with manual translations for the new sgame additions.
- Reset the 16 new language files to match the English key set as a temporary placeholder because automated translation services are currently blocked by rate limits/captcha.

## Engine/runtime changes
- `embed.py` now embeds file bytes as hex escapes to preserve UTF-8 content regardless of compiler/source encoding.
- `meson.build` embeds `assets/localization/loc_english.txt` into the client build as `res_loc_english`.
- `src/common/files.c` registers the embedded English localization as a builtin file so `Loc_ReloadFile()` can load it via the standard filesystem search.

## Localization key coverage
- Added missing sgame keys to `assets/localization/loc_english.txt`:
  - `g_not_giveable`
  - `g_harvester_need_skulls`
  - `g_oneflag_need_flag`
- Added the same keys to `loc_french.txt`, `loc_german.txt`, `loc_italian.txt`, `loc_spanish.txt`, and `loc_russian.txt` with manual translations to avoid raw key fallbacks.
- Verified all `loc_*.txt` files now contain the full key set from `loc_english.txt`.

## Translation status and blocker
- Google Translate endpoints are blocked by a CAPTCHA response in this environment.
- MyMemory free API quota was exhausted during testing and enforces a low daily cap.
- As a result, the 16 new language files were reset to English (correct keys, English values) pending access to a translation source.

## Follow-up needed to complete translations
- Provide translation files for the 16 new languages, or
- Supply an API key for a translation service with higher limits (LibreTranslate/MyMemory paid key, etc.), or
- Approve running a local/offline translation tool with downloadable models.
