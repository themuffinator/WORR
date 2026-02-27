#!/usr/bin/env python3

from __future__ import annotations

import argparse
import datetime as dt
import json
import pathlib
import subprocess
import sys
from typing import Any

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from tools.release.targets import get_target


def run_command(command: list[str]) -> None:
    subprocess.run(command, check=True)


def metadata_artifacts(target: dict[str, Any]) -> list[dict[str, str]]:
    artifacts: list[dict[str, str]] = []
    for role in ("client", "server"):
        config = target[role]
        artifacts.append(
            {
                "name": config["package_name"],
                "kind": "archive",
                "role": role,
            }
        )
        artifacts.append(
            {
                "name": config["manifest_name"],
                "kind": "manifest",
                "role": role,
            }
        )
    installer = target.get("installer")
    if installer:
        artifacts.append(
            {
                "name": installer["name"],
                "kind": installer["type"],
                "role": "installer",
            }
        )
    return artifacts


def main() -> int:
    parser = argparse.ArgumentParser(description="Package release assets for a target platform.")
    parser.add_argument("--input-dir", required=True, help="Staged install directory")
    parser.add_argument("--output-dir", required=True, help="Release output directory")
    parser.add_argument("--platform-id", required=True, help="Release platform id")
    parser.add_argument("--repo", required=True, help="GitHub owner/repo")
    parser.add_argument("--channel", required=True, help="Release channel")
    parser.add_argument("--version", required=True, help="Version string")
    parser.add_argument("--commit-sha", default="", help="Source commit sha")
    parser.add_argument("--build-id", default="", help="Build id for traceability")
    parser.add_argument("--allow-prerelease", action="store_true", help="Enable prerelease updates in config")
    parser.add_argument("--write-config", dest="write_config", action="store_true", default=True,
                        help="Write updater config into staged input")
    parser.add_argument("--no-write-config", dest="write_config", action="store_false",
                        help="Do not write updater config into staged input")
    parser.add_argument("--metadata-path", help="Output metadata file path")
    parser.add_argument("--installer-path", help="Optional installer path to include if present")
    args = parser.parse_args()

    target = get_target(args.platform_id)
    input_dir = pathlib.Path(args.input_dir).resolve()
    output_dir = pathlib.Path(args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    package_script = pathlib.Path(__file__).resolve().parents[1] / "package_release.py"
    if not package_script.is_file():
        raise SystemExit(f"Packaging script not found: {package_script}")

    for role in ("client", "server"):
        config = target[role]
        command = [
            sys.executable,
            str(package_script),
            "--input-dir",
            str(input_dir),
            "--output-dir",
            str(output_dir),
            "--package-name",
            config["package_name"],
            "--manifest-name",
            config["manifest_name"],
            "--version",
            args.version,
            "--repo",
            args.repo,
            "--channel",
            args.channel,
            "--launch-exe",
            config["launch_exe"],
            "--archive-format",
            target["archive_format"],
            "--platform-id",
            target["platform_id"],
            "--platform-os",
            target["os"],
            "--platform-arch",
            target["arch"],
        ]
        if args.allow_prerelease:
            command.append("--allow-prerelease")
        if args.write_config:
            command.append("--write-config")
        run_command(command)

    metadata_path = pathlib.Path(args.metadata_path).resolve() if args.metadata_path else (
        output_dir / f"metadata-{target['platform_id']}.json"
    )
    installer_present = False
    installer_name = None
    if args.installer_path:
        installer = pathlib.Path(args.installer_path).resolve()
        if installer.is_file():
            installer_present = True
            installer_name = installer.name

    artifacts = metadata_artifacts(target)
    if target.get("installer") and not installer_present:
        artifacts = [entry for entry in artifacts if entry["role"] != "installer"]
    if installer_present and installer_name:
        for entry in artifacts:
            if entry["role"] == "installer":
                entry["name"] = installer_name

    metadata = {
        "schema_version": 1,
        "generated_at_utc": dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "platform_id": target["platform_id"],
        "os": target["os"],
        "arch": target["arch"],
        "channel": args.channel,
        "version": args.version,
        "commit_sha": args.commit_sha,
        "build_id": args.build_id,
        "archive_format": target["archive_format"],
        "autoupdater": target["autoupdater"],
        "artifacts": artifacts,
    }
    metadata_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(f"Wrote {metadata_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
