Build Warning Cleanup 2026-01-15
================================

Context
-------
The Windows clang build emitted warnings from read-only sources (`src/game` and
`q2proto`) plus a `q_noreturn` mismatch in `src/server/game3_proxy`. This change
set resolves those warnings without altering read-only code.

Changes
-------
- `src/server/game3_proxy/game3_proxy.c`: `wrap_error` now calls
  `q_unreachable()` after `Com_Error` so the `q_noreturn` annotation matches the
  control flow.
- `meson.build`: added `_CRT_NONSTDC_NO_WARNINGS` for win32 builds to silence
  `strdup` deprecation warnings coming from read-only `src/game` sources.
- `meson.build`: `q2proto` sources are compiled as `libq2proto.a` with
  `-Wno-unused-function` (gcc/clang syntax) to suppress unused static function
  warnings in the read-only `q2proto` subtree. The main executables link against
  `libq2proto.a`.

Notes
-----
- Use `tools\\meson_setup.ps1 compile -C builddir` so the resource compiler
  environment is configured correctly on Windows.
