# Pic Leading Slash Paths

## Issue
- `R_RegisterImage` required explicit extensions for leading-slash pic names,
  so references like `/tags/default` failed with `Invalid quake path` even
  though `tags/default.png` exists in the base data.
- Rooted pic references need to resolve against non-`pics/` assets (for example,
  `tags/` and `players/`), while still allowing extension fallback.

## Change
- `R_RegisterImage` now applies the default `.pcx` extension to rooted pic
  paths when one is not supplied, enabling fallback to other formats like
  `.png` in the same directory.

## Impact
- Pic lookups like `/tags/default` resolve to `tags/default.png` via extension
  fallback, avoiding `Invalid quake path` errors.
- Rooted pic references continue to load assets outside `pics/` without
  requiring callers to specify explicit extensions.
