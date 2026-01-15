#!/usr/bin/env python3

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import pathlib
import sys
import zipfile


DEFAULT_PRESERVE = [
    "worr_update.json",
    "worr_updater.exe",
    "baseq2/*.cfg",
    "baseq2/autoexec.cfg",
    "baseq2/config.cfg",
    "baseq2/saves/*",
    "baseq2/screenshots/*",
    "baseq2/demos/*",
    "baseq2/logs/*",
]


def hash_file(path: pathlib.Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def should_include(rel_path: str, include: list[str], exclude: list[str]) -> bool:
    if include:
        if not any(fnmatch.fnmatch(rel_path, pat) for pat in include):
            return False
    if exclude:
        if any(fnmatch.fnmatch(rel_path, pat) for pat in exclude):
            return False
    return True


def write_update_config(args: argparse.Namespace, root: pathlib.Path) -> None:
    config = {
        "repo": args.repo,
        "channel": args.channel,
        "manifest_asset": args.manifest_name,
        "package_asset": args.package_name,
        "launch_exe": args.launch_exe,
        "autolaunch": args.autolaunch,
        "allow_prerelease": args.allow_prerelease,
        "preserve": args.preserve or DEFAULT_PRESERVE,
    }

    config_path = root / "worr_update.json"
    config_path.write_text(json.dumps(config, indent=2), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Package WORR release assets with manifest.")
    parser.add_argument("--input-dir", required=True, help="Staging install directory root")
    parser.add_argument("--output-dir", required=True, help="Output directory for artifacts")
    parser.add_argument("--package-name", required=True, help="Zip asset name")
    parser.add_argument("--manifest-name", required=True, help="Manifest asset name")
    parser.add_argument("--version", required=True, help="Release version (semver)")
    parser.add_argument("--repo", required=True, help="GitHub owner/repo")
    parser.add_argument("--channel", default="stable", help="Release channel name")
    parser.add_argument("--launch-exe", required=True, help="Binary to launch after update")
    parser.add_argument("--autolaunch", dest="autolaunch", action="store_true", default=True,
                        help="Default autolaunch checkbox")
    parser.add_argument("--no-autolaunch", dest="autolaunch", action="store_false",
                        help="Disable autolaunch checkbox")
    parser.add_argument("--allow-prerelease", action="store_true", help="Allow prerelease updates")
    parser.add_argument("--include", action="append", default=[], help="Glob pattern to include")
    parser.add_argument("--exclude", action="append", default=[], help="Glob pattern to exclude")
    parser.add_argument("--preserve", action="append", default=[], help="Preserve patterns for updater config")
    parser.add_argument("--write-config", action="store_true", help="Write worr_update.json into input dir")

    args = parser.parse_args()

    input_dir = pathlib.Path(args.input_dir).resolve()
    output_dir = pathlib.Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.write_config:
        write_update_config(args, input_dir)

    files: list[dict[str, object]] = []
    paths: list[pathlib.Path] = []

    for root, _, filenames in os.walk(input_dir):
        root_path = pathlib.Path(root)
        for filename in filenames:
            full_path = root_path / filename
            rel_path = full_path.relative_to(input_dir).as_posix()
            if rel_path in (args.package_name, args.manifest_name):
                continue
            if not should_include(rel_path, args.include, args.exclude):
                continue
            paths.append(full_path)
            files.append({
                "path": rel_path,
                "sha256": hash_file(full_path),
                "size": full_path.stat().st_size,
            })

    files.sort(key=lambda entry: entry["path"])

    package_path = output_dir / args.package_name
    with zipfile.ZipFile(package_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for full_path in paths:
            rel_path = full_path.relative_to(input_dir).as_posix()
            archive.write(full_path, rel_path)

    manifest = {
        "version": args.version,
        "package": {
            "name": args.package_name,
            "sha256": hash_file(package_path),
            "size": package_path.stat().st_size,
        },
        "files": files,
    }

    manifest_path = output_dir / args.manifest_name
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    print(f"Wrote {package_path}")
    print(f"Wrote {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
