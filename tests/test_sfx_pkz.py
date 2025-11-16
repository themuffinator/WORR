#!/usr/bin/env python3
from __future__ import annotations

import argparse
import io
import os
import pathlib
import subprocess
import sys
import tempfile
import zipfile


STUB = b"#!/bin/sh\n# q2pro sfx stub\n"
CONFIG_NAME = "testsfx.cfg"
CONFIG_CONTENT = "echo SFX_PKZ_OK\n"


def build_sfx_pkz(target: pathlib.Path) -> None:
    archive = io.BytesIO()
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr(CONFIG_NAME, CONFIG_CONTENT)
    target.write_bytes(STUB + archive.getvalue())


def run_test(worr_ded: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = pathlib.Path(tmpdir)
        base = tmp_path / "baseq2"
        home = tmp_path / "home"
        base.mkdir(parents=True)
        home.mkdir(parents=True)
        pkz_path = base / "pak0_sfx.pkz"
        build_sfx_pkz(pkz_path)

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
            "+exec",
            CONFIG_NAME,
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
        if "SFX_PKZ_OK" not in output:
            raise SystemExit(f"expected marker missing from output\n{output}")
        if "extra bytes at the beginning" not in output:
            raise SystemExit(f"zip64 locator warning missing\n{output}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("worr_ded", type=pathlib.Path)
    args = parser.parse_args()
    run_test(args.worr_ded)


if __name__ == "__main__":
    main()
