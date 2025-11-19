#!/usr/bin/python3

# Helper: Copy DLL built by game subproject to Meson output directory

import argparse
import os
import pathlib
import shutil
import stat
import sys

args = argparse.ArgumentParser()
args.add_argument('output_name', help="output file name")
args.add_argument('game_dll', help="game shared library path")
arg_values = args.parse_args()

game_dll_path = pathlib.Path(arg_values.game_dll)
dest_path = pathlib.Path(arg_values.output_name)
output_path = dest_path.parent

if not game_dll_path.is_file():
    print(f"Source DLL not found: {game_dll_path}", file=sys.stderr)
    sys.exit(1)

try:
    output_path.mkdir(parents=True, exist_ok=True)
except OSError as exc:
    print(f"Failed to create output directory {output_path}: {exc}", file=sys.stderr)
    sys.exit(1)

if not output_path.is_dir():
    print(f"Output path is not a directory: {output_path}", file=sys.stderr)
    sys.exit(1)

try:
    shutil.copy2(game_dll_path, dest_path)
except OSError as exc:
    print(f"Failed to copy {game_dll_path} to {dest_path}: {exc}", file=sys.stderr)
    sys.exit(1)

# Additional file extensions which are copied over as well, if existing.
# Currently, Windows debug symbols.
ADDITIONAL_EXTS = [".pdb"]
for ext in ADDITIONAL_EXTS:
    candidate = game_dll_path.with_suffix(ext)
    if not candidate.exists():
        continue
    if not candidate.is_file():
        print(f"Symbol file is not a regular file: {candidate}", file=sys.stderr)
        sys.exit(1)

    dest_path = output_path / candidate.name
    try:
        shutil.copy2(candidate, dest_path)
    except OSError as exc:
        print(f"Failed to copy {candidate} to {dest_path}: {exc}", file=sys.stderr)
        sys.exit(1)
