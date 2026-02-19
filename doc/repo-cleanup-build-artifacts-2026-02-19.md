# Repository Cleanup: Build/Generated Artifacts (2026-02-19)

## Summary
This cleanup removed stale generated build outputs and local IDE/cache artifacts from the repository root to keep the workspace lean and reduce accidental churn in commits.

## Removed directories
The following root-level generated directories were deleted:

- `.codex_temp/`
- `.vs/`
- `build-cfg-test-hbargs/`
- `build-cfg-test-openalwarn/`
- `build-check-native/`
- `build-check-native-cmd/`
- `build-check-native-list/`
- `build-check-native-rcenv/`
- `build-check-native-test/`
- `build-check-native-windresenv/`
- `build-check-native2/`
- `build-clean-20260206/`
- `build-clean-native-20260206b/`
- `build-clean-native-20260206c/`
- `build-clean-native-20260206d/`
- `build-clean-native-20260206e/`
- `build-clean-native-20260206f/`
- `build-clean-native-20260206g/`
- `build-clean-native-20260206i/`
- `builddir/`
- `builddir.failed/`
- `builddir.failed2/`
- `builddir.old/`

Note: pre-cleanup sizing indicated these artifacts consumed several GB of disk space, dominated by the historical `builddir*` and `build-clean-native-*` trees.

## Ignore policy updates
`.gitignore` was expanded to prevent these artifacts from reappearing in future status output:

- Added local workspace ignores: `.vs/`, `.codex_temp/` (while preserving existing `.vscode/`).
- Added root build tree ignores: `/build*/`, `/builddir*/`.
- Added Meson/Ninja generated files: `/meson-private/`, `/meson-logs/`, `/compile_commands.json`, `/.ninja_log`, `/.ninja_deps`.

## Compatibility impact
No runtime/source functionality changed. This is strictly repository hygiene (generated files and ignore rules).
