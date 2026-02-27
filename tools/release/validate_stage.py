#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import sys

if __package__ is None or __package__ == "":
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from tools.release.targets import get_target


def require_file(path: pathlib.Path) -> None:
    if not path.is_file():
        raise SystemExit(f"Missing required file: {path}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate staged release contents for a release target.")
    parser.add_argument("--install-dir", required=True, help="Staged install directory (.install)")
    parser.add_argument("--base-game", default="baseq2", help="Base game directory name")
    parser.add_argument("--platform-id", required=True, help="Release platform id")
    args = parser.parse_args()

    install_dir = pathlib.Path(args.install_dir).resolve()
    if not install_dir.is_dir():
        raise SystemExit(f"Install directory not found: {install_dir}")

    target = get_target(args.platform_id)

    require_file(install_dir / target["client"]["launch_exe"])
    require_file(install_dir / target["server"]["launch_exe"])

    base_game_dir = install_dir / args.base_game
    if not base_game_dir.is_dir():
        raise SystemExit(f"Missing base game directory: {base_game_dir}")

    base_files = list(base_game_dir.rglob("*"))
    base_files = [path for path in base_files if path.is_file()]
    if not base_files:
        raise SystemExit(f"Base game directory is empty: {base_game_dir}")

    updater = target.get("autoupdater", {})
    updater_asset = updater.get("updater_asset")
    if updater_asset:
        require_file(install_dir / updater_asset)

    print(f"Validated staged install for {args.platform_id}: {install_dir}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # pragma: no cover
        print(f"Validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
