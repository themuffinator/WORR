# Main menu fixed-position alignment correction (2026-01-29)

## Overview
- Fixed-position menu items now use the active UI virtual coordinate system directly
  (no extra 640x480 centering offsets).
- Plaque/logo fixed draws now respect their declared positions as-is, with optional
  right anchoring against the current virtual width.

## Rationale
- The UI renderer already operates in a virtual coordinate space derived from the
  current resolution. Adding extra centering offsets caused misalignment.

## Code changes
- Removed virtual screen offset math from fixed bitmap layout, hit-test, and draw paths.
- Fixed items now render at their configured x/y in `uis.width`/`uis.height` space.
