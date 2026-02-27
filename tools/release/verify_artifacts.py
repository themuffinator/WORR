#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import sys
from typing import Any

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from tools.release.targets import TARGETS, get_target, expected_asset_names


def choose_targets(platform_ids: list[str]) -> list[dict[str, Any]]:
    if platform_ids:
        return [get_target(platform_id) for platform_id in platform_ids]
    return list(TARGETS)


def require_asset(artifacts_root: pathlib.Path, name: str) -> pathlib.Path:
    direct = artifacts_root / name
    if direct.is_file():
        return direct

    for match in artifacts_root.rglob(name):
        if match.is_file():
            return match

    raise FileNotFoundError(name)


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify release artifact bundles before publishing.")
    parser.add_argument("--artifacts-root", required=True, help="Downloaded artifacts root directory")
    parser.add_argument(
        "--platform-id",
        action="append",
        default=[],
        help="Optional platform id filter (repeatable)",
    )
    args = parser.parse_args()

    artifacts_root = pathlib.Path(args.artifacts_root).resolve()
    if not artifacts_root.is_dir():
        raise SystemExit(f"Artifacts root not found: {artifacts_root}")

    targets = choose_targets(args.platform_id)
    failures: list[str] = []

    for target in targets:
        platform_id = target["platform_id"]
        expected = [f"metadata-{platform_id}.json", *expected_asset_names(target)]
        for name in expected:
            try:
                require_asset(artifacts_root, name)
            except FileNotFoundError:
                failures.append(f"{platform_id}: missing {name}")

    if failures:
        print("Artifact verification failed:")
        for line in failures:
            print(f"- {line}")
        return 1

    print(f"Verified release artifacts in {artifacts_root}")
    for target in targets:
        print(f"- {target['platform_id']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
