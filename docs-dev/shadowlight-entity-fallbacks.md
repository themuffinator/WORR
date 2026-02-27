# Shadowlight entity fallbacks

## Overview
- Map `light` and `dynamic_light` entities now accept `radius` and `light` as
  fallback sources for shadow light radius when `shadowlightradius` is not set.
- `shadowlightintensity` defaults to `1.0` when omitted, so lights with only a
  radius no longer evaluate to zero intensity.

## Details
- Spawn parsing stores the `light` key in spawn temp so it can be used as a
  shadow light fallback value.
- `setup_dynamic_light` now resolves shadow radius using:
  1. `shadowlightradius` (if explicitly provided)
  2. `radius` (if provided)
  3. `light` (if provided)
- If none of these are present or the resolved radius is `<= 0`, the entity
  does not create a shadow light.

## Impact
- Existing maps that already use `shadowlightradius` behave the same.
- Maps that only set `radius` or `light` now spawn the corresponding shadow
  light without needing extra keys.
