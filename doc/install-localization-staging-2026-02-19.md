# .install Localization Staging Fix (2026-02-19)

## Summary
`.install` staging now copies localization source files directly into the staged game tree:

- Source: `assets/localization/`
- Target: `.install/baseq2/localization/`

This runs as part of `tools/stage_install.py`, so localization files are present immediately after the staging step instead of only existing inside `worr-assets.pkz`.

## Problem
The local build pipeline stages runtime files with:

1. `tools/stage_install.py`
2. `tools/package_assets.py`

Before this change:

- Stage step copied runtime binaries and `builddir/baseq2` into `.install/`.
- `builddir/baseq2` did not contain `localization/`.
- Asset packaging generated `.install/baseq2/worr-assets.pkz` but did not materialize loose localization files in `.install/baseq2/localization/`.

Result: `.install` was refreshed without loose localization text files.

## Implementation
`tools/stage_install.py` now includes explicit localization staging:

- Added `--assets-dir` argument (default: `assets`).
- Added `copy_localization_tree(...)` to copy `assets/localization` into `.install/<base-game>/localization`.
- Added validation:
  - Fails if the source localization directory is missing.
  - Fails if no localization files are found.
- Added staging output log with copied localization file count.

No changes were made to `tools/package_assets.py`; asset archive generation still runs as before.

## Validation
After running:

```powershell
python tools/stage_install.py --build-dir builddir --install-dir .install --base-game baseq2
```

the staged output now includes:

- `.install/baseq2/localization/loc_english.txt`
- `.install/baseq2/localization/loc_french.txt`
- `.install/baseq2/localization/loc_german.txt`
- ...and the rest of the `loc_*.txt` set.

`tools/package_assets.py` can still run after staging to produce:

- `.install/baseq2/worr-assets.pkz`

## Files Changed
- `tools/stage_install.py`
- `doc/install-localization-staging-2026-02-19.md`
