# Nightly Release Automation + Versioning Policy (2026-02-19)

## Summary
This change set introduces a full nightly release pipeline that:

- builds WORR for all defined supported CI platforms,
- validates staged distributables,
- packages archives and installers per platform,
- publishes a nightly GitHub prerelease only when there were commits during the UTC day,
- generates release notes with a full changelog section,
- emits a machine-readable release index that tells autoupdaters exactly which assets belong to which platform.

It also updates build workflows to always refresh `.install/` and package assets there, matching the repository staging rule.

## Versioning Policy Update
`version.py` now supports channel-aware automated version generation.

### New inputs
- `WORR_CHANNEL`: release channel (`stable` default, `nightly` supported).
- `WORR_NIGHTLY_DATE`: nightly date stamp (`YYYYMMDD`), defaults to current UTC date.

### Nightly semver shape
Nightly builds now produce semver-compatible prerelease identifiers:

- `<major>.<minor>.<patch>-nightly.<YYYYMMDD>.r<revision-padded>`
- example: `0.0.0-nightly.20260219.r00001234`

This keeps versions monotonic for update comparison and reproducible per commit/day.

### MSI versioning output
`version.py --msi-version` now emits a strict `Major.Minor.Patch` value for MSI packaging.

- Stable channel: uses `VERSION` file patch.
- Nightly channel: patch derives from git revision count (clamped to MSI constraints).

This avoids prerelease string failures in MSI tooling while preserving upgrade ordering for nightly installers.

## New Release Tooling
New scripts under `tools/release/`:

1. `targets.py`
   - Single source of truth for release targets/platforms.
   - Defines runner, OS/arch, archive format, package names, manifest names, installer expectations, and autoupdater mode.
   - Provides matrix JSON for GitHub Actions.

2. `prepare_nightly.py`
   - Determines if nightly should run (`commits today > 0`, UTC) with optional force override.
   - Outputs nightly tag/name/date metadata.

3. `validate_stage.py`
   - Validates staged `.install/` runtime layout per platform target before packaging.

4. `package_platform.py`
   - Packages client/server assets according to target registry.
   - Writes per-platform metadata JSON used for release aggregation.

5. `compose_release_index.py`
   - Aggregates per-platform metadata into a single cross-platform release index.
   - Resolves actual files, computes hashes/sizes, and outputs canonical asset list.

6. `generate_release_notes.py`
   - Builds release notes with:
     - build metadata,
     - compare link,
     - full commit changelog list,
     - per-platform asset inventory.

## Packaging Format Enhancements
`tools/package_release.py` now supports:

- archive format selection (`zip` and `tar.gz`),
- platform metadata fields,
- build metadata fields,
- richer manifest metadata (`repo`, `channel`, `generated_at_utc`, `package.format`).

The existing manifest fields required by the updater remain intact (`version`, `package`, `files`) to preserve compatibility.

## Updater Compatibility Improvements
`src/updater/worr_updater.c` release parsing was hardened:

- release array parsing now scans all returned releases, not only the first element,
- candidate release must include both manifest and package assets,
- channel matching is applied using `config.channel` against release tags (stable remains permissive).

This prevents nightly/stable asset collisions when using GitHub’s multi-release API responses.

## `.install/` Workflow Enforcement
`release.yml` was updated to align with the staging rule:

- build outputs are staged into `.install/` via `tools/stage_install.py`,
- assets are packaged into `.install/baseq2/worr-assets.pkz`,
- release packaging and MSI generation now consume `.install/`.

## New Nightly Workflow
Added: `.github/workflows/nightly.yml`

### Triggering
- Scheduled daily (UTC) run.
- Manual dispatch with `force` override.

### Execution flow
1. Prepare nightly metadata and matrix.
2. Build all target platforms (Windows, Linux, macOS) in parallel.
3. Stage `.install/` and package `worr-assets.pkz`.
4. Validate staged output.
5. Build Windows MSI.
6. Package client/server release archives + manifests per target.
7. Upload platform bundles as artifacts.
8. Aggregate bundles into a release index and release notes.
9. Publish/update nightly prerelease with all assets and changelog body.

### Commit gating behavior
Nightly release is skipped automatically when no commits landed during the current UTC day (unless manual `force` is enabled).

## Autoupdater Distribution Mapping
The nightly pipeline now produces a canonical release index (`worr-release-index-nightly.json`) that maps:

- platform id,
- archive/installer assets,
- manifest assets,
- hashes/sizes,
- autoupdater mode per platform.

This index is the future-proof contract for platform-aware autoupdaters to resolve the correct distributable automatically.

## Files Added
- `.github/workflows/nightly.yml`
- `tools/release/__init__.py`
- `tools/release/targets.py`
- `tools/release/prepare_nightly.py`
- `tools/release/validate_stage.py`
- `tools/release/package_platform.py`
- `tools/release/compose_release_index.py`
- `tools/release/generate_release_notes.py`
- `docs-dev/nightly-release-automation-2026-02-19.md`

## Files Updated
- `version.py`
- `tools/package_release.py`
- `tools/stage_install.py`
- `src/updater/worr_updater.c`
- `.github/workflows/release.yml`

