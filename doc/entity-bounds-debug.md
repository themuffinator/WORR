# Map entity bounds debug

## Overview
Adds a server-side debug cvar to draw bounds and/or origins for non-client
map entities each frame.

## Usage
- `g_show_entity_bounds 1` draws bounding boxes.
- `g_show_entity_bounds 2` draws origins.
- `g_show_entity_bounds 3` draws both.

Notes:
- Only in-use non-client entities are drawn.
- Bounds are drawn only when the entity is linked.
- Bounds render in yellow and origins render in cyan.

Files: `src/game/sgame/g_main.cpp`.
