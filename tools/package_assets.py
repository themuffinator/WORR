#!/usr/bin/env python3

from __future__ import annotations

import argparse
import pathlib
import zipfile


def collect_files(root: pathlib.Path) -> list[pathlib.Path]:
    return sorted(path for path in root.rglob('*') if path.is_file())


def main() -> int:
    parser = argparse.ArgumentParser(description='Package WORR runtime assets into a baseq2 archive.')
    parser.add_argument('--assets-dir', default='assets', help='Source assets directory')
    parser.add_argument('--install-dir', default='.install', help='Install staging directory')
    parser.add_argument('--base-game', default='baseq2', help='Base game directory name')
    parser.add_argument('--archive-name', default='worr-assets.pkz', help='Output archive filename')
    args = parser.parse_args()

    assets_dir = pathlib.Path(args.assets_dir).resolve()
    install_dir = pathlib.Path(args.install_dir).resolve()
    output_dir = install_dir / args.base_game
    archive_path = output_dir / args.archive_name

    if not assets_dir.is_dir():
        raise SystemExit(f'Assets directory not found: {assets_dir}')

    files = collect_files(assets_dir)
    if not files:
        raise SystemExit(f'No files found in assets directory: {assets_dir}')

    output_dir.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(archive_path, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            archive.write(path, path.relative_to(assets_dir).as_posix())

    print(f'Wrote {archive_path}')
    print(f'Packed {len(files)} files from {assets_dir}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
