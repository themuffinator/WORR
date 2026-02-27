#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from typing import Any


TARGETS: list[dict[str, Any]] = [
    {
        "platform_id": "windows-x86_64",
        "runner": "windows-latest",
        "os": "windows",
        "arch": "x86_64",
        "archive_format": "zip",
        "client": {
            "package_name": "worr-client-win64.zip",
            "manifest_name": "worr-client-win64.json",
            "launch_exe": "worr.exe",
        },
        "server": {
            "package_name": "worr-server-win64.zip",
            "manifest_name": "worr-server-win64.json",
            "launch_exe": "worr.ded.exe",
        },
        "installer": {
            "type": "msi",
            "name": "worr-win64.msi",
        },
        "autoupdater": {
            "mode": "native",
            "updater_asset": "worr_updater.exe",
            "config_asset": "worr_update.json",
        },
    },
    {
        "platform_id": "linux-x86_64",
        "runner": "ubuntu-latest",
        "os": "linux",
        "arch": "x86_64",
        "archive_format": "tar.gz",
        "client": {
            "package_name": "worr-client-linux-x86_64.tar.gz",
            "manifest_name": "worr-client-linux-x86_64.json",
            "launch_exe": "worr",
        },
        "server": {
            "package_name": "worr-server-linux-x86_64.tar.gz",
            "manifest_name": "worr-server-linux-x86_64.json",
            "launch_exe": "worr.ded",
        },
        "installer": None,
        "autoupdater": {
            "mode": "archive_sync",
            "updater_asset": None,
            "config_asset": "worr_update.json",
        },
    },
    {
        "platform_id": "macos-x86_64",
        "runner": "macos-13",
        "os": "macos",
        "arch": "x86_64",
        "archive_format": "tar.gz",
        "client": {
            "package_name": "worr-client-macos-x86_64.tar.gz",
            "manifest_name": "worr-client-macos-x86_64.json",
            "launch_exe": "worr",
        },
        "server": {
            "package_name": "worr-server-macos-x86_64.tar.gz",
            "manifest_name": "worr-server-macos-x86_64.json",
            "launch_exe": "worr.ded",
        },
        "installer": None,
        "autoupdater": {
            "mode": "archive_sync",
            "updater_asset": None,
            "config_asset": "worr_update.json",
        },
    },
]


def get_target(platform_id: str) -> dict[str, Any]:
    for target in TARGETS:
        if target["platform_id"] == platform_id:
            return target
    raise KeyError(f"Unknown platform id: {platform_id}")


def matrix_payload() -> dict[str, Any]:
    include = []
    for target in TARGETS:
        include.append(
            {
                "platform_id": target["platform_id"],
                "runner": target["runner"],
                "os": target["os"],
                "archive_format": target["archive_format"],
                "has_installer": bool(target["installer"]),
            }
        )
    return {"include": include}


def expected_asset_names(target: dict[str, Any]) -> list[str]:
    assets = [
        target["client"]["package_name"],
        target["client"]["manifest_name"],
        target["server"]["package_name"],
        target["server"]["manifest_name"],
    ]
    installer = target.get("installer")
    if installer:
        assets.append(installer["name"])
    return assets


def main() -> int:
    parser = argparse.ArgumentParser(description="WORR release target registry.")
    parser.add_argument("--matrix-json", action="store_true", help="Print GitHub matrix JSON")
    parser.add_argument("--platform", help="Platform id to print")
    parser.add_argument("--assets", action="store_true", help="Print expected asset names for --platform")
    parser.add_argument("--pretty", action="store_true", help="Pretty-print JSON output")
    args = parser.parse_args()

    indent = 2 if args.pretty else None

    if args.matrix_json:
        print(json.dumps(matrix_payload(), indent=indent))
        return 0

    if args.platform:
        target = get_target(args.platform)
        if args.assets:
            for name in expected_asset_names(target):
                print(name)
        else:
            print(json.dumps(target, indent=indent))
        return 0

    print(json.dumps({"targets": TARGETS}, indent=indent))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

