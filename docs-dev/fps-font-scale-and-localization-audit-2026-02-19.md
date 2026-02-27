# FPS Font Scale and Localization Audit (2026-02-19)

## Scope
- Reduce FPS counter font footprint.
- Audit localization key coverage and verify English contains all keys used by any locale.

## FPS Counter Change

### Goal
Render the FPS readout at a smaller, less intrusive size.

### Implementation
- File: `src/game/cgame/cg_draw.cpp`
- Function: `CG_DrawFPS`
- Change:
  - `k_fps_line_height_px` changed from `5.0f` to `4.0f`.
  - Added an inline comment documenting the intent.

This path already computes a scale factor from font line height and applies it
through `SCR_SetScale`, so reducing the target line height is the lowest-risk
way to reduce FPS text size.

## Localization Audit

### Files audited
- All `assets/localization/loc_*.txt` files.

### Audit method
- Parsed each non-empty, non-comment line with key assignment format:
  - `key = "value"`
  - `key <variant> = "value"`
- Built:
  - English key set (`loc_english.txt`)
  - Union of keys across all locale files
- Compared union against English key set.

### Results
- English unique keys: `2889`
- Union unique keys across all locales: `2889`
- Missing in English: `0`
- Non-matching assignment lines per locale: `0`
- Duplicate key definitions per locale: `2`
  - `m_always_run_joy`
  - `m_delete`

### Conclusion
- English fully covers all localization keys present in the repository.
- No missing English key entries were found.
- Existing duplicate keys are consistent across all locale files and were not
  modified in this pass.

