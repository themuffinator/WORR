# Windows Resource Compiler Setup

This project compiles Windows resource files (`.rc`) for the client, server,
and bundled dependencies. Meson requires a Windows resource compiler to be
available during `meson setup`. If one is not found, configuration fails with:

```
src/windows/meson.build:12:22: ERROR: Could not find Windows resource compiler
```

To make this deterministic on Windows, the repo includes:

- `tools/rc.cmd`: a small wrapper that locates `llvm-rc.exe` in common LLVM
  install locations or on PATH.
- `meson.native.ini`: a Meson native file that points the `rc` binary at
  `tools/rc.cmd`.

## Usage

```
meson setup --native-file meson.native.ini builddir
```

## Notes

- If you prefer the MSVC `rc.exe`, point `rc` at it in `meson.native.ini`.
- If LLVM is installed elsewhere, update `tools/rc.cmd` accordingly.
