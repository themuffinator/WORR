#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import shutil


def copy_runtime_files(build_dir: pathlib.Path, install_dir: pathlib.Path) -> int:
    copied = 0
    patterns = ('worr*.exe', 'worr*.dll', 'worr*.pdb')
    for pattern in patterns:
        for path in sorted(build_dir.glob(pattern)):
            shutil.copy2(path, install_dir / path.name)
            copied += 1
    return copied


def copy_base_game_tree(build_dir: pathlib.Path, install_dir: pathlib.Path, base_game: str) -> None:
    source = build_dir / base_game
    target = install_dir / base_game
    if not source.is_dir():
        raise SystemExit(f'Expected staged game directory not found: {source}')
    shutil.copytree(source, target)


def copy_localization_tree(assets_dir: pathlib.Path, install_dir: pathlib.Path, base_game: str) -> int:
    source = assets_dir / 'localization'
    target = install_dir / base_game / 'localization'

    if not source.is_dir():
        raise SystemExit(f'Localization directory not found: {source}')

    localization_files = sorted(path for path in source.rglob('*') if path.is_file())
    if not localization_files:
        raise SystemExit(f'No localization files found in: {source}')

    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)
    return len(localization_files)


def main() -> int:
    parser = argparse.ArgumentParser(description='Stage WORR runtime distributables into .install/.')
    parser.add_argument('--build-dir', default='builddir', help='Meson build directory')
    parser.add_argument('--assets-dir', default='assets', help='Repository assets directory')
    parser.add_argument('--install-dir', default='.install', help='Output staging directory')
    parser.add_argument('--base-game', default='baseq2', help='Base game directory name')
    args = parser.parse_args()

    build_dir = pathlib.Path(args.build_dir).resolve()
    assets_dir = pathlib.Path(args.assets_dir).resolve()
    install_dir = pathlib.Path(args.install_dir).resolve()

    if not build_dir.is_dir():
        raise SystemExit(f'Build directory not found: {build_dir}')

    if install_dir.exists():
        shutil.rmtree(install_dir)
    install_dir.mkdir(parents=True, exist_ok=True)

    copied_runtime = copy_runtime_files(build_dir, install_dir)
    if copied_runtime == 0:
        raise SystemExit(f'No runtime binaries found in {build_dir}')

    copy_base_game_tree(build_dir, install_dir, args.base_game)
    copied_localization = copy_localization_tree(assets_dir, install_dir, args.base_game)

    print(f'Wrote staged runtime to {install_dir}')
    print(f'Copied {copied_runtime} root runtime binaries')
    print(f'Copied {copied_localization} localization files')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
