# Teamplay Outline Visibility

## Overview
Defines the teamplay-specific behavior for player outlines and rim lights,
including visibility through walls and PVS handling.

## Client Behavior
- Team colors: outlines and rim lights use red for team 1 and blue for team 2
  when `player_state_t::team_id` is non-zero. Non-teamplay falls back to red.
- Allied outlines: teammates receive an outline pass when
  `cl_enemy_outline` is enabled; the outline ignores depth to show through
  walls.
- Allied rim lighting: teammates also receive rim lighting (team colored),
  but it remains depth tested and does not render through walls.
- The renderer uses `RF_OUTLINE_NODEPTH` to disable depth testing for allied
  outlines.
- Transparency: outline and rim alpha are multiplied by the final per-entity
  alpha (invisibility or thirdperson fade).
- Corpse suppression: body-queue entities are excluded by entity number, and
  re-release dead POI markers (`player_skinnum_t::poi_icon`) disable effects.

## Server Behavior
- Allied players bypass PVS culling in `SV_BuildClientFrame` when their
  `team_id` matches the client. This keeps teammate entities available so
  outlines and markers render even when occluded.

## Notes
- Team metadata (team index and dead POI) is only available with the
  re-release protocol; older protocols default to red and cannot distinguish
  allies.
