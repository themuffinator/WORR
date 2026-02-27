# Say/Say_Team and Location Support

## Overview
- Adds game-side `say` and `say_team` commands with OpenTDM-style macro expansion.
- Adds Q3-style `target_location` plus server-side `.loc` load/auto-write.
- Adds explicit `target_position` spawn handling for parity.

## Chat Behavior
- `say` prefix: `<name>: `
- `say_team` prefix: `(<name>): `
- Flood control uses `CheckFlood` after message build.
- Newline appended to each message.
- Recipients: all connected clients for `say`; same team (plus self) for `say_team`.
- Dedicated server logs chat lines to the console.

## Macro Tokens
- `%h` / `%H`: `H:<health>`
- `#h`: `<health>`
- `%a`: `A:<armor>` or `A:<armor> P:<cells>` when power armor exists
- `#a`: `<armor>`
- `%A`: `A:<armor> <armor name>` (+ power armor cells when present)
- `%w`: short weapon (abbrev) or `abbr:ammo`
- `%W`: long weapon name or `name:ammo`
- `%l`: location string (see selection rules below)
- `%n`: nearby visible teammates
- `%N`: nearby visible players
- Macros expand only inside the message payload (prefix preserved).
- Payload is capped to 256 chars before expansion; expansion aborts if it would overflow.

## Location Selection
- Prefer `target_location`: closest in PVS, fallback to closest by distance.
- Else use `.loc`: closest in PVS, fallback to closest by distance.
- Else use nearest high-value item (weapon/powerup/mega health) in PVS, with
  `upper`/`lower` modifier when multiple of the same type exist.
- Final fallback: `water` if in water, otherwise the map name.

## .loc Handling
- Loads `locs/<map>.loc` on map start (server-side).
- If no `.loc` file exists but `target_location` entities do, auto-writes
  `locs/<map>.loc` from those entities (coords scaled by 8).

## Target Entities
- `target_position` now uses its own spawn handler (same behavior as `info_notnull`).
- `target_location` stores message and origin for chat location lookup.
