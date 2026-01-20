# Player Brightskins

## Summary
- Adds an optional brightskin overlay for player skins using `_brtskn` PNGs.
- The overlay is a fullbright additive pass tinted by client-configured colors.

## Asset Naming
- `players/<model>/<skin>_brtskn.png` (optional).
- `players/<model>/disguise_brtskn.png` when disguise skins are active.

## Cvars
- `cl_brightskins_custom` (0/1, default 0): 0 uses team mapping (free=green, red=red, blue=blue). 1 uses custom colors.
- `cl_brightskins_enemy_color` (hex, default `#ff0000`): enemy color when custom is enabled.
- `cl_brightskins_team_color` (hex, default `#00ff00`): team/self color when custom is enabled.
- `cl_brightskins_dead` (0/1, default 1): when 1, skip brightskins for dead players (requires rerelease team info).

## Rendering Behavior
- Brightskins render as a separate translucent fullbright pass with additive blending and no depth writes.
- The tint comes from the selected cvar color; 8-digit hex can include alpha to attenuate brightness.
- Entity alpha (invisibility/third-person) multiplies the brightskin pass.

## Notes
- RF_CUSTOMSKIN overrides do not attempt brightskin overlays.
- Spectators ignore `cl_brightskins_custom` and use the default team mapping.
