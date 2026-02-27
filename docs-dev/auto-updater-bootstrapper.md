# Auto-Updater Bootstrapper + Release Pipeline

This change set adds a standalone updater binary, a release manifest format, and a GitHub Actions workflow to package and publish releases with consistent version metadata.

## Bootstrapper Overview
- Binary: `worr_updater.exe` (Windows GUI, separate from the engine; Windows-only for now).
- Behavior:
  - Reads `worr_update.json` from the install root.
  - Fetches the latest release from GitHub and downloads the manifest asset.
  - Compares the local binary version (from file version metadata) to the manifest version.
  - Downloads and verifies the release package (SHA-256).
  - Extracts into a staging directory and syncs files into the install root.
  - Deletes files not present in the manifest (unless preserved).
  - Launches the target binary with original arguments when the checkbox is enabled.
- UI:
  - Status text (“Checking for updates...”, “Downloading...”, “Updating...”, “Installing...”, “Loading...”).
  - Three progress bars (download, update/extract, install/sync).
  - Checkbox to auto-launch after update completes.
- The updater skips overwriting its own executable during sync to avoid file locks.

## Updater Config: `worr_update.json`
This file ships with the release and can be customized per install.

Example:
```json
{
  "repo": "themuffinator/WORR",
  "channel": "stable",
  "manifest_asset": "worr-client-win64.json",
  "package_asset": "worr-client-win64.zip",
  "launch_exe": "worr.exe",
  "autolaunch": true,
  "allow_prerelease": false,
  "preserve": [
    "worr_update.json",
    "worr_updater.exe",
    "baseq2/*.cfg",
    "baseq2/autoexec.cfg",
    "baseq2/config.cfg",
    "baseq2/saves/*",
    "baseq2/screenshots/*",
    "baseq2/demos/*",
    "baseq2/logs/*"
  ]
}
```

Notes:
- `manifest_asset` and `package_asset` must match the release asset names.
- `preserve` is used to avoid deleting or overwriting user data when syncing.
- `allow_prerelease` toggles whether the updater should use the full release list (array) vs `releases/latest`.

## Release Manifest Format
The updater downloads a JSON manifest asset and uses it to validate and sync files.

Example:
```json
{
  "version": "1.2.3",
  "package": {
    "name": "worr-client-win64.zip",
    "sha256": "abc123...",
    "size": 12345678
  },
  "files": [
    { "path": "worr.exe", "sha256": "deadbeef...", "size": 123456 },
    { "path": "baseq2/gamex86_64.dll", "sha256": "feedface...", "size": 456789 }
  ]
}
```

## Versioning System
- `VERSION` holds the base semver (e.g., `1.2.3`).
- `version.py` outputs:
  - `--version`: display string used by the build (adds dev/build metadata when not a release build).
  - `--semver`: stable semver string for release artifacts.
  - `--revision`: git commit count for Windows file version fields.
  - `--major/--minor/--patch/--prerelease`: individual components.
- Release builds are identified via `WORR_RELEASE=1` (CI sets this for release workflow).
- Windows resources now emit `VERSION_MAJOR.VERSION_MINOR.VERSION_PATCH.REVISION` for file/product versions.

## Packaging Script
`tools/package_release.py` creates both the zip and manifest.

Example usage:
```sh
python tools/package_release.py \
  --input-dir dist \
  --output-dir dist \
  --package-name worr-client-win64.zip \
  --manifest-name worr-client-win64.json \
  --version 1.2.3 \
  --repo themuffinator/WORR \
  --channel stable \
  --launch-exe worr.exe \
  --write-config
```

## GitHub Actions Release Workflow
- Workflow: `.github/workflows/release.yml`
- Triggers:
  - Push to `main` with `[release]` in the commit message.
  - Manual `workflow_dispatch`.
- Release commits should bump `VERSION` to the intended semver.
- Steps:
  - Build via Meson (MSYS2 + MinGW).
  - Install into a staging directory.
  - Package client/server assets and manifests.
  - Build `worr-win64.msi` via WiX.
  - Create a GitHub release and upload assets.

Build option:
- `-Dbootstrapper=true` toggles building `worr_updater.exe` (Windows only).

## MSI Installer
- WiX template: `tools/installer/worr.wxs`
- Build script: `tools/build_msi.ps1`
- Output asset: `worr-win64.msi`

## Third-Party Code
- `miniz` is included under `src/updater/third_party` for ZIP extraction.
