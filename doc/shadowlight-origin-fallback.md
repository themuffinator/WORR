# Shadowlight Origin Fallback

## Overview
Shadowlight entities were only used when the entity was present in the
current server frame. If a light entity was culled or dropped from the
packet list, its light disappeared even though its configstring parameters
were available. This prevented map shadowlights from showing reliably.

## Change
- Shadowlights now source their origin and color from the most recent entity
  state when available.
- If the entity has never been updated in the current session, the light
  falls back to the entity baseline so static lights still spawn.
- The strict `serverframe == current frame` requirement is removed, allowing
  static shadowlights to remain active even when their entities are omitted
  from the current packet.

## Notes
This keeps shadowlights stable for static map entities while still honoring
updated positions for moving lights.

Files: `src/client/effects.c`.
