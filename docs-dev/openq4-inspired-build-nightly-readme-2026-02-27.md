# OpenQ4-Inspired Build and Release Refinements (2026-02-27)

## Objective

Adopt strong workflow ideas from `../OpenQ4/` into WORR without copying project-specific behavior:

- tighter `.install/` staging discipline,
- clearer post-build install flow,
- stronger nightly release verification,
- clearer README onboarding for build/install/nightly behavior.

## Key Changes

### 1) Unified post-build staging command

Added `tools/refresh_install.py` as the single entry point for local/CI staging.

What it does:

1. Runs `tools/stage_install.py` to rebuild `.install/` from the current build output.
2. Runs `tools/package_assets.py` to generate `.install/<base-game>/worr-assets.pkz`.
3. Optionally runs `tools/release/validate_stage.py` when a `--platform-id` is provided.

Why:

- Removes duplicated command chains across workflows.
- Ensures post-build staging and asset packaging are always coupled.
- Keeps `.install/` refresh behavior consistent between local runs and CI.

### 2) Nightly release payload verification hardening

Added `tools/release/verify_artifacts.py`.

What it validates:

- per-target metadata files (`metadata-<platform>.json`),
- expected platform release assets from `tools/release/targets.py`
  (client archive/manifest, server archive/manifest, installer where required).

Why:

- Prevents publishing partial nightlies when one platform artifact is missing.
- Mirrors OpenQ4’s explicit artifact existence checks before release publication.

### 3) Workflow integration improvements

Updated `.github/workflows/nightly.yml`:

- Replaced duplicated stage/package/validate blocks with `tools/refresh_install.py`.
- Added explicit `git fetch --force --tags origin` before release note generation.
- Added `verify_artifacts.py` gate before composing release index.
- Added workflow run metadata (`run_id`, `run_url`) into generated release notes.

Updated `.github/workflows/release.yml`:

- Replaced manual staging commands with `tools/refresh_install.py --platform-id windows-x86_64`.

### 4) README/BUILDING onboarding improvements

Updated `README.md`:

- Added quick navigation links near the top.
- Added `Quick Start` with configure/build/refresh flow.
- Added `.install/` staging section defining expected behavior and outputs.
- Added `Nightly Builds` section describing schedule, matrix build, staging/validation, packaging, and publish flow.

Updated `BUILDING.md`:

- Added `Local distributable staging` section with concrete Linux/macOS and Windows commands.
- Documented that post-build staging now refreshes `.install/` in one command.

## Adopted Ideas (from OpenQ4, adapted for WORR)

- One high-confidence post-build command for staging and packaging.
- Explicit release-artifact completeness checks before publish.
- README emphasis on quick-start operational flow, not just architecture/goals.
- Nightly release notes with workflow traceability metadata.

## Notes

- This keeps WORR’s release model and file naming intact.
- No changes were made to `q2proto/`.
- Vulkan renderer path behavior was not altered.
