#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import sys
import tempfile

HISTORY_FILE = ".syshistory"
BASEGAME = "baseq2"


def run_test(worr_ded: pathlib.Path) -> None:
    if os.name != "nt":
        print("Skipping Windows-only test", file=sys.stderr)
        return

    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = pathlib.Path(tmpdir)
        base = tmp_path / BASEGAME
        home = tmp_path / "home"
        base.mkdir(parents=True)
        home.mkdir(parents=True)

        home_backslash = str(home).replace("/", "\\")

        cmd = [
            str(worr_ded),
            "+set",
            "basedir",
            str(tmp_path),
            "+set",
            "homedir",
            home_backslash,
            "+set",
            "game",
            BASEGAME,
            "+set",
            "fs_autoexec",
            "0",
            "+set",
            "developer",
            "1",
            "+set",
            "sys_history",
            "8",
            "+quit",
        ]

        env = os.environ.copy()
        env.setdefault("HOME", str(home))

        proc = subprocess.run(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
            env=env,
        )

        output = proc.stdout
        if proc.returncode != 0:
            raise SystemExit(
                f"worr.ded exited with {proc.returncode}\n--- OUTPUT ---\n{output}\n------------"
            )

        history_dir = home / BASEGAME
        history_file = history_dir / HISTORY_FILE
        if not history_dir.is_dir():
            raise SystemExit(f"history directory missing: {history_dir}")
        if not history_file.is_file():
            raise SystemExit(f"history file missing: {history_file}\n{output}")

        # Ensure the file can be read even if it is empty
        history_file.read_text()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("worr_ded", type=pathlib.Path)
    args = parser.parse_args()
    run_test(args.worr_ded)


if __name__ == "__main__":
    main()
