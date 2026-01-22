#!/usr/bin/python3

# loosely based on the following answer on SO: https://stackoverflow.com/a/61128721

import os, sys, shutil

# get absolute input and output paths
input_path = sys.argv[1]
output_path = sys.argv[2]

# make sure destination directory exists
dest_dir = os.path.dirname(output_path)
if len(dest_dir) > 0:
    os.makedirs(dest_dir, exist_ok = True)

# read in the file as bytes
with open(input_path, "rb") as in_file:
    in_contents = in_file.read()

# convert
size = len(in_contents)
chunks = []
chunk_size = 32
for i in range(0, size, chunk_size):
    chunk = in_contents[i:i + chunk_size]
    chunks.append(''.join(f'\\x{b:02x}' for b in chunk))

out_lines = [
    '#include <stddef.h>',
    f'const char {sys.argv[3]}[] ='
]
out_lines.extend([f'    "{chunk}"' for chunk in chunks])
out_lines.append(';')
out_lines.append(f'const size_t {sys.argv[3]}_size = {size};')
out_contents = '\n'.join(out_lines)

# write
with open(output_path, "w", encoding="utf-8", newline="\n") as out_file:
    out_file.write(out_contents)
