# Wheel Hint Offset, AA, and Attack Behavior

## Hint Position
- Drop hint block now starts half a font line lower (adds `line_height / 2` to
  the initial Y coordinate) to better align with the wheel center region.

## Triangle Smoothing
- `DrawPolygon()` now supersamples 2×2 subpixels per pixel to anti-alias the
  pointer triangle edges while preserving the gradient.

## Input Behavior
- `+attack` no longer selects or closes the wheel; selection only occurs on
  wheel close (release) using the current hover.
