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

# read in the file
in_file = open(input_path, "r")
in_contents = in_file.read()
in_file.close()

# convert
size = len(in_contents);
in_contents = in_contents.replace('\\', '\\\\')
in_contents = in_contents.replace('\n', '\\n')
in_contents = in_contents.replace('\r', '\\r')
in_contents = in_contents.replace('"', '\\"')

var_name = sys.argv[3]

converted = [
    '#include <stddef.h>',
    f'const char {var_name}[] ='
]

chunk_size = 16384
for i in range(0, len(in_contents), chunk_size):
    chunk = in_contents[i:i + chunk_size]
    converted.append(f'"{chunk}"')

converted.append(';')
converted.append(f'const size_t {var_name}_size = {size};')
in_contents = '\n'.join(converted)

# write
out_file = open(output_path, "w")
out_contents = out_file.write(in_contents)
out_file.close()
