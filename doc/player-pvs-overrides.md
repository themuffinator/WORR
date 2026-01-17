# Player PVS Overrides

## Overview
Ensures player entities remain visible to spectators and dead players regardless
of the normal PVS culling rules. This keeps player outlines, markers, and chase
camera views consistent even when targets are outside the current PVS.

## Behavior
- Spectators: all player entities bypass PVS culling, including when the
  spectator is in chase cam (PM_FREEZE).
- Dead players: while dead, all player entities bypass PVS culling for that
  client.
- Non-player entities continue to use normal PVS/PHS checks.

## Implementation
- Server-side override in `SV_BuildClientFrame`:
  - The client is considered a spectator if
    `pm_type == PM_SPECTATOR` or `pm_type == PM_FREEZE` (chase cam).
  - The client is considered dead if `pm_type == PM_DEAD/PM_GIB`.
  - When either condition is true, player entities (`ent->client` non-null)
    skip PVS culling for that client.
