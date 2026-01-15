# Weapon Bar Safe Zone Support

## Summary
All weapon bar modes now respect `scr_safe_zone`. Placement and clamping
are calculated within the safe region so bars stay inside the configured
HUD margins.

## Affected layouts
- Static bars (modes 1-3): left/right anchors now offset from the safe
  region, and vertical/horizontal centering occurs within the safe bounds.
- Timed bars (modes 4-5): horizontal centering and vertical placement are
  computed inside the safe area. Weapon name placement is clamped to the
  safe top edge.

## Notes
Safe zone margins are derived from `scr_safe_zone` using the same fraction
as other HUD elements.
