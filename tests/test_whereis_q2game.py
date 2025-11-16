import argparse
import os
import pathlib
import subprocess
import tempfile
import zipfile

CONFIG_NAME = "testwhereis.cfg"
PACK_ENTRY = "docs/test_whereis.txt"
PACK_MARKER = "Q2Game.kpf"


def build_q2game_kpf(target: pathlib.Path) -> None:
    with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr(PACK_ENTRY, "from_kpf\n")


def run_test(worr_ded: pathlib.Path) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_path = pathlib.Path(tmpdir)
        baseq2 = tmp_path / "baseq2"
        home = tmp_path / "home"
        baseq2.mkdir()
        home.mkdir()

        # create physical copy that should lose to the Q2Game pack when using /path lookups
        physical = baseq2 / PACK_ENTRY
        physical.parent.mkdir(parents=True, exist_ok=True)
        physical.write_text("from_disk\n", encoding="utf-8")

        build_q2game_kpf(tmp_path / PACK_MARKER)

        config_path = baseq2 / CONFIG_NAME
        config_path.write_text(f"whereis /{PACK_ENTRY} all\n", encoding="utf-8")

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

        matches = [line for line in output.splitlines() if PACK_ENTRY in line]
        if len(matches) < 2:
            raise SystemExit(f"expected both pack and physical hits\n{output}")
        if PACK_MARKER not in matches[0]:
            raise SystemExit(f"expected Q2Game hit first\n{output}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("worr_ded", type=pathlib.Path)
    args = parser.parse_args()
    run_test(args.worr_ded)


if __name__ == "__main__":
    main()
