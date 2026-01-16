#!/usr/bin/python3

# Helper: Copy game DLLs to the Meson output directory

import argparse
import pathlib
import shutil

args = argparse.ArgumentParser()
args.add_argument('output_name', help="output file name or stamp path")
args.add_argument('game_dll', help="game shared library path")
args.add_argument('--copy-dir', dest='copy_dir', help="optional extra output directory")
args.add_argument('--stamp', action='store_true', help="write a stamp file instead of copying to output_name")
arg_values = args.parse_args()

game_dll_path = pathlib.Path(arg_values.game_dll)
output_path = pathlib.Path(arg_values.output_name)
output_dir = output_path.parent

# Additional file extensions which are copied over as well, if existing.
# Currently, Windows debug symbols.
ADDITIONAL_EXTS = [".pdb"]


def copy_bundle(dest_dir, dll_name):
    dest_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(game_dll_path, dest_dir / dll_name)
    for ext in ADDITIONAL_EXTS:
        candidate = game_dll_path.with_suffix(ext)
        if candidate.exists():
            shutil.copy2(candidate, dest_dir / candidate.name)


dll_name = output_path.name
if arg_values.stamp:
    dll_name = game_dll_path.name

if arg_values.stamp:
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path.touch()
else:
    copy_bundle(output_dir, dll_name)

if arg_values.copy_dir:
    extra_dir = pathlib.Path(arg_values.copy_dir)
    if extra_dir.resolve() != output_dir.resolve():
        copy_bundle(extra_dir, dll_name)
