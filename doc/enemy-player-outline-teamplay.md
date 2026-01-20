# Teamplay Outline Visibility

## Overview
Defines the teamplay-specific behavior for player outlines and rim lights,
including visibility through walls and PVS handling.

## Client Behavior
- Colors follow the brightskin selection (custom colors when enabled; otherwise
  free=green, team1=red, team2=blue).
- Teammate outlines are controlled by `cl_player_outline_team` and render
  through walls via `RF_OUTLINE_NODEPTH`.
- Enemy outlines are controlled by `cl_player_outline_enemy` and remain depth
  tested.
- Rim lighting uses `cl_player_rimlight_enemy` and `cl_player_rimlight_team` and
  always remains depth tested.
- The local player receives teammate outline/rim lighting in thirdperson view
  when the team cvars are enabled.
- Outline and rim alpha are multiplied by the final per-entity alpha
  (invisibility or thirdperson fade) and the brightskin color alpha.
- Corpse suppression: body-queue entities are excluded, and re-release dead POI
  markers (`player_skinnum_t::poi_icon`) disable effects.

## Server Behavior
- Allied players bypass PVS culling in `SV_BuildClientFrame` when their
  `team_id` matches the client. This keeps teammate entities available so
  outlines and markers render even when occluded.

## Notes
- Team metadata (team index and dead POI) is only available with the
  re-release protocol; older protocols default to treating all players
  as enemies.
