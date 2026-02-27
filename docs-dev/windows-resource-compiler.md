# Windows Resource Compiler Setup

This project compiles Windows resource files (`.rc`) for the client, server,
and bundled dependencies. Meson requires a Windows resource compiler to be
available during `meson setup`. If one is not found, configuration fails with:

```
src/windows/meson.build:12:22: ERROR: Could not find Windows resource compiler
```

To make this deterministic on Windows, the repo includes:

- `tools/rc.cmd`: a small wrapper that locates `llvm-rc.exe` in common LLVM
  install locations or on PATH, and falls back to `rc.exe` from the Windows
  SDK if available.
- `tools/meson_setup.ps1` and `tools/meson_setup.cmd`: wrappers that set the
  `WINDRES` environment variable to `tools/rc.cmd` before invoking Meson.
- `meson.native.ini`: optional Meson native file for the `lld` link args.

## Usage

```
tools\meson_setup.ps1 setup --native-file meson.native.ini builddir
```

Or from cmd.exe:

```
tools\meson_setup.cmd setup --native-file meson.native.ini builddir
```

## Notes

- If you prefer the MSVC `rc.exe`, set `WINDRES` to it instead of `tools/rc.cmd`.
- If LLVM is installed elsewhere, update `tools/rc.cmd` accordingly.
- MSYS2/MinGW shells already provide `windres` on PATH, so the wrappers are not
  required there.
