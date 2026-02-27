#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import shutil


def copy_runtime_files(build_dir: pathlib.Path, install_dir: pathlib.Path) -> int:
    copied = 0
    copied_paths: set[pathlib.Path] = set()

    runtime_names = ('worr', 'worr.ded', 'worr_updater')
    runtime_patterns = ('worr*.exe', 'worr*.dll', 'worr*.pdb', 'worr*.so', 'worr*.dylib')

    for name in runtime_names:
        path = build_dir / name
        if path.is_file() and path not in copied_paths:
            shutil.copy2(path, install_dir / path.name)
            copied_paths.add(path)
            copied += 1

    for pattern in runtime_patterns:
        for path in sorted(build_dir.glob(pattern)):
            if not path.is_file() or path in copied_paths:
                continue
            shutil.copy2(path, install_dir / path.name)
            copied_paths.add(path)
            copied += 1
    return copied


def copy_base_game_tree(build_dir: pathlib.Path, install_dir: pathlib.Path, base_game: str) -> None:
    source = build_dir / base_game
    target = install_dir / base_game
    if not source.is_dir():
        raise SystemExit(f'Expected staged game directory not found: {source}')
    shutil.copytree(source, target)


def copy_assets_tree(assets_dir: pathlib.Path, install_dir: pathlib.Path, base_game: str) -> int:
    if not assets_dir.is_dir():
        raise SystemExit(f'Assets directory not found: {assets_dir}')

    asset_files = sorted(path for path in assets_dir.rglob('*') if path.is_file())
    if not asset_files:
        raise SystemExit(f'No asset files found in: {assets_dir}')

    target_root = install_dir / base_game
    for entry in sorted(assets_dir.iterdir()):
        target = target_root / entry.name
        if entry.is_dir():
            if target.exists():
                shutil.rmtree(target)
            shutil.copytree(entry, target)
        elif entry.is_file():
            shutil.copy2(entry, target)

    return len(asset_files)


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
    copied_assets = copy_assets_tree(assets_dir, install_dir, args.base_game)

    print(f'Wrote staged runtime to {install_dir}')
    print(f'Copied {copied_runtime} root runtime binaries')
    print(f'Copied {copied_assets} asset files')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
