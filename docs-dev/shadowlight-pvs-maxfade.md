# Shadowlight PVS Cap and Max Fade Distance

## Overview
- Shadowlight entities now cap the number of PVS-visible lights processed per frame at 64.
- New `shadowlightmaxfadedistance` key allows hard distance culling on the client.

## PVS Cap
- `CL_AddShadowLights` tracks shadowlights whose entities are present in the current server frame.
- Only the first 64 such lights are processed; additional PVS-visible shadowlights are skipped.
- Baseline-only shadowlights are still allowed, but can be culled by max fade distance.

## Max Fade Distance Key
- New map key: `shadowlightmaxfadedistance`.
- When set to a value > 0, the client skips the shadowlight if the camera is farther than this distance.
- This is separate from `shadowlightstartfadedistance` / `shadowlightendfadedistance` and is a hard cutoff.

## Networking / Compatibility
- Shadowlight configstrings include the new max fade distance field.
- Clients accept both old (no max fade distance) and new formats; old data defaults to 0 (disabled).

## Manual Check
- Add `shadowlightmaxfadedistance` to a map light and verify it disappears beyond that range.
- Place >64 shadowlights in view and confirm only 64 are processed.
