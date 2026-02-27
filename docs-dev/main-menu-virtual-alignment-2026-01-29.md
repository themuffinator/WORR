# Main menu fixed-position virtual alignment (2026-01-29)

## Overview
- Keep /gfx/ paths for menu background images to avoid lookup regressions.
- Ensure fixed-position bitmap widgets use their virtual-screen coordinates for culling and hit-testing.

## UI JSON change
- Main menu buttons now keep `/gfx/menu_bg_off` and `/gfx/menu_bg_on` explicitly.

## Code change
- Menu hit-test/draw now uses fixed bitmap y-coordinates for visibility checks,
  so absolute positions align correctly in the virtual screen space.
