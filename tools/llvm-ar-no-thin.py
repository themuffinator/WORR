#!/usr/bin/env python3
import os
import subprocess
import sys


def strip_thin_flags(argv):
    filtered = [arg for arg in argv if arg not in ("-T", "--thin")]
    if not filtered:
        return filtered
    flags = filtered[0]
    if flags and not flags.startswith("-") and "T" in flags:
        filtered[0] = flags.replace("T", "")
        if not filtered[0]:
            filtered = filtered[1:]
    elif flags.startswith("-") and flags[1:].isalpha() and "T" in flags:
        filtered[0] = flags.replace("T", "")
        if filtered[0] == "-":
            filtered = filtered[1:]
    return filtered


def main():
    args = strip_thin_flags(sys.argv[1:])
    llvm_ar = os.environ.get("LLVM_AR", "llvm-ar")
    return subprocess.call([llvm_ar] + args)


if __name__ == "__main__":
    raise SystemExit(main())
