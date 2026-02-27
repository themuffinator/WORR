# Builtin Localization Embedding (2026-01-27)

## Summary
- Embedded all `assets/localization/loc_*.txt` files into the client binary so
  `loc_language` changes always resolve, even when no external localization
  files are present on disk.
- Extended the builtin file table to expose every localization file under
  `localization/loc_*.txt` via the virtual filesystem search paths.

## Rationale
The language menu updates `loc_language`, which selects a `loc_file`. Until now
only `loc_english.txt` was embedded, so any other language failed to resolve and
fell back to English. Embedding all localization files ensures language changes
work in developer builds and packaged builds that do not ship a standalone
`localization/` directory.

## Implementation Details
- `meson.build`: generate embedded C sources for every localization file using
  `embed.py`, exporting `res_loc_<language>` symbols and adding them to
  `client_src`.
- `src/common/files.c`: added forward declarations and builtin file table
  entries for all embedded `localization/loc_*.txt` files.

## Notes
- Builtin files remain lowest priority in the search path, so external
  localization files (if present in packs or directories) still override these
  embedded defaults.

## Files Touched
- `meson.build`
- `src/common/files.c`
