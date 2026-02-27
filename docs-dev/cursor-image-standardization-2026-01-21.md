WORR cursor image standardization (2026-01-21)

Summary
- Standardize UI cursor assets on /gfx/cursor.png (Q2Game.kpf, 16x16).

Details
- Updated legacy UI initialization to register /gfx/cursor.png as the default cursor image for fullscreen cursor rendering.
- Updated cgame UI initialization to register /gfx/cursor.png for menu cursor rendering (maintaining UI_CURSOR_SIZE = 16).
- Updated WORR menu definitions (script and JSON) to reference /gfx/cursor.png so scripted menus and JSON menus align with the new cursor asset.
