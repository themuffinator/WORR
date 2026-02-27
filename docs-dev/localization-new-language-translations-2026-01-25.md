# Localization translations for new languages (2026-01-25)

## Scope
- Translated the 16 new localization files added in the expansion (Arabic, Bulgarian, Chinese Simplified/Traditional, Czech, Danish, Dutch, Finnish, Japanese, Korean, Norwegian, Polish, Portuguese-BR, Portuguese-PT, Swedish, Turkish).
- Backfilled the 198 `cg_auto_*`/`e_auto_*` keys that existed only in English into all non-English files, preserving key parity.

## Translation sources
- Google Translate (`translate.googleapis.com` endpoint) was used for:
  - The Arabic full file.
  - Missing `cg_auto_*`/`e_auto_*` keys in French, German, Italian, Spanish, and Russian.
- Argos Translate offline models were used for the remaining new-language files after hitting Google rate limits.

## Placeholder handling
- Preserved `fmt` placeholders (`{}`, `{0}`), printf tokens (`%s`, `%d`, etc.), `%bind:` tokens, `$` localization references, and `\n`/`\t` escapes during translation.
- Normalized `%bind` tokens after translation to avoid whitespace inside the macro and keep runtime parsing consistent.

## Key parity
- All `loc_*.txt` files now include the exact English key set; no missing or extra keys remain.

## Notes
- Machine translations should be reviewed by native speakers where possible.
- Key names were left unchanged (English-based IDs) to maintain compatibility with existing servers, demos, and tooling.
