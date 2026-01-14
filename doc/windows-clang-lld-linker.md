# Windows clang lld linker

## Problem
- Windows builds with clang failed when linking OpenAL utilities (LNK1107 invalid/corrupt file reading libcommon.a).
- The clang++ driver defaulted to MSVC link.exe, which cannot consume .a archives produced by llvm-ar (thin archives).

## Fix
- Force clang to use lld-link for both C and C++ links.
- Current build uses Meson options:
  - meson setup builddir --reconfigure --native-file meson.native.ini -Dc_link_args=-fuse-ld=lld -Dcpp_link_args=-fuse-ld=lld
- The native file now carries the same intent for fresh build directories.
- Rebuild: meson compile -C builddir

## Notes
- If the builddir was previously built with link.exe, run `ninja -C builddir clean` before rebuilding to avoid stale archives.
- lld-link warns about `--nxcompat` and related flags; these are benign for now.
