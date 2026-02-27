OpenAL-Soft Warning Resolution

Overview
- Resolve OpenAL-Soft warnings on Windows toolchains without lowering warning
  levels for the subproject.
- Keep the main project warning behavior unchanged.

Changes
- `subprojects/openal-soft-1.23.1/common/alnumeric.h`: remove the space in
  user-defined literal operators (`operator""_i64`, `operator""_u64`) to avoid
  the deprecated literal operator warning.
- `subprojects/openal-soft-1.23.1/alc/alc.cpp`: use `_wfopen_s` under MSVC
  toolchains to avoid `_wfopen` deprecation warnings while preserving behavior.

Why
- These warnings originate in vendored OpenAL-Soft sources and clutter builds
  on Windows. The changes keep behavior identical while removing the warnings.
