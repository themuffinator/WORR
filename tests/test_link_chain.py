#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import pathlib
import subprocess
import tempfile


def run_test(worr_ded: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = pathlib.Path(tmpdir)
        base = tmp_path / "baseq2"
        home = tmp_path / "home"
        config_dir = base / "configs"
        config_dir.mkdir(parents=True)
        home.mkdir(parents=True)

        target_cfg = config_dir / "link_target.cfg"
        target_cfg.write_text("echo LINK_CHAIN_OK\n", encoding="utf-8")

        cmd = [
            str(worr_ded),
            "+set",
            "basedir",
            str(tmp_path),
            "+set",
            "homedir",
            str(home),
            "+set",
            "game",
            "baseq2",
            "+set",
            "fs_autoexec",
            "0",
            "+set",
            "developer",
            "1",
            "+link",
            "configs/link_a.cfg",
            "configs/link_b.cfg",
            "+link",
            "configs/link_b.cfg",
            "configs/link_target.cfg",
            "+exec",
            "configs/link_a.cfg",
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
        if "LINK_CHAIN_OK" not in output:
            raise SystemExit(f"expected marker missing from output\n{output}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("worr_ded", type=pathlib.Path)
    args = parser.parse_args()
    run_test(args.worr_ded)


if __name__ == "__main__":
    main()
