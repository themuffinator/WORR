#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def run_step(label: str, command: list[str]) -> None:
    print(f"[refresh-install] {label}", flush=True)
    subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Refresh the local .install staging root with current WORR binaries and assets."
    )
    parser.add_argument("--build-dir", default="builddir", help="Meson build directory")
    parser.add_argument("--assets-dir", default="assets", help="Repository assets directory")
    parser.add_argument("--install-dir", default=".install", help="Install staging directory")
    parser.add_argument("--base-game", default="baseq2", help="Base game directory name")
    parser.add_argument(
        "--archive-name",
        default="worr-assets.pkz",
        help="Generated asset archive name inside <install>/<base-game>",
    )
    parser.add_argument(
        "--platform-id",
        default="",
        help="Optional release platform id for post-stage validation (for example windows-x86_64)",
    )
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parent.parent
    tools_dir = repo_root / "tools"
    python_exe = pathlib.Path(sys.executable).resolve()

    stage_script = tools_dir / "stage_install.py"
    assets_script = tools_dir / "package_assets.py"
    validate_script = tools_dir / "release" / "validate_stage.py"

    run_step(
        "Stage runtime and base game tree",
        [
            str(python_exe),
            str(stage_script),
            "--build-dir",
            args.build_dir,
            "--assets-dir",
            args.assets_dir,
            "--install-dir",
            args.install_dir,
            "--base-game",
            args.base_game,
        ],
    )

    run_step(
        "Package staged runtime assets",
        [
            str(python_exe),
            str(assets_script),
            "--assets-dir",
            args.assets_dir,
            "--install-dir",
            args.install_dir,
            "--base-game",
            args.base_game,
            "--archive-name",
            args.archive_name,
        ],
    )

    if args.platform_id:
        run_step(
            f"Validate staged payload ({args.platform_id})",
            [
                str(python_exe),
                str(validate_script),
                "--install-dir",
                args.install_dir,
                "--base-game",
                args.base_game,
                "--platform-id",
                args.platform_id,
            ],
        )

    install_dir = pathlib.Path(args.install_dir).resolve()
    print(f"[refresh-install] Completed: {install_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
