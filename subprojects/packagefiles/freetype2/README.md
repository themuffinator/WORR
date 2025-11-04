# Freetype2 packagefile

This packagefile wires Meson's CMake subproject helper into the upstream
`freetype` build to produce a static library suitable for use through the
`freetype2.wrap` bundle.

## Inputs

* The build runs in CMake mode and forces a static library by setting
  `BUILD_SHARED_LIBS=OFF`. Optional consumers such as BZip2, Brotli, HarfBuzz,
  PNG, and Zlib are disabled by default so the wrap does not rely on system
  packages that may be unavailable in consuming projects.【F:subprojects/packagefiles/freetype2/meson.build†L1-L18】
* When a Meson-side Zlib dependency is available (either from the system or the
  `zlib-ng` fallback), the packagefile allows CMake to find and consume it,
  otherwise Zlib support remains disabled.【F:subprojects/packagefiles/freetype2/meson.build†L12-L18】

## Exported targets

* The wrap exports a `freetype2` Meson dependency named `freetype_dep`, which is
  re-used by projects via `dependency('freetype2')` through the `[provide]`
  mapping in `freetype2.wrap`.【F:subprojects/freetype2.wrap†L9-L10】

## WrapDB submission status

* Status: Pending submission (update once the WrapDB merge request is opened).
* Tracking: TODO – file the WrapDB MR/issue per the contribution guide and
  record the link here for future maintainers.
