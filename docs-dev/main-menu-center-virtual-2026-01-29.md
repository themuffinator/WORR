# Main menu centered virtual anchors (2026-01-29)

## Overview
- Treat fixed-position menu items as coordinates in the centered virtual screen space by default.
- Support left/right anchoring for fixed-position items and fixed plaque/logo if needed.

## UI behavior
- Fixed-position items (including main menu buttons) now offset by the centered virtual screen origin.
- Anchors: center (default), left, right.

## Code changes
- BitmapWidget layout now applies centered virtual offsets when fixed positioning is used.
- Menu draw/hit-test/culling paths apply the same virtual y-offset for fixed items.
- Menu can apply left/right anchoring for fixed plaque/logo when configured.
