# Obituary Feed Time Units Compatibility (2026-01-22)

## Problem
The kill feed appeared to show only one entry at a time when `cg_obituary_time`
was configured with small values (for example `3`), because the cvar was
interpreted strictly as milliseconds and clamped as an integer. That made
entries expire almost immediately.

## Change
Obituary timing cvars now accept seconds for small values while keeping
backward-compatible millisecond handling for large values:
- Values below 100 are treated as seconds and converted to milliseconds.
- Values at or above 100 are treated as milliseconds.

This avoids accidental 3–5 ms lifetimes while preserving existing configs that
already use millisecond values like `3000` or `200`.

## Examples
- `cg_obituary_time 3` => 3000 ms
- `cg_obituary_fade 0.2` => 200 ms
- `cg_obituary_time 3000` => 3000 ms (legacy)

## Files
- src/game/cgame/cg_draw.cpp
