# Quake Live Crosshair Cvars (WORR Port)

## Summary
- Adds the Quake Live crosshair cvars using the `cl_` prefix.
- Replaces `crosshairscale` and `ch_scale` with `cl_crosshairSize`.
- `cl_crosshairSize` also drives hit marker sizing.
- Replaces legacy `ch_*` crosshair color channels with palette + brightness controls.
- Implements hit styles, palette coloring, and pulse behavior.

## Cvars
- `cl_crosshairBrightness` (0.0-1.0, default 1.0)
  - Multiplies the RGB channels of the active crosshair color.
- `cl_crosshairColor` (1-26, default 25)
  - Selects the base crosshair color from the Quake Live palette.
- `cl_crosshairHealth` (0/1, default 0)
  - When enabled, health-based coloring overrides `cl_crosshairColor`.
- `cl_crosshairHitColor` (1-26, default 1)
  - Palette index used by hit styles 2, 5, and 8.
- `cl_crosshairHitStyle` (0-8, default 2)
  - Defines hit feedback behavior. See the Hit Styles section.
- `cl_crosshairHitTime` (milliseconds, default 200)
  - Duration for the hit effect (color and pulse) when a hit is registered.
- `cl_crosshairPulse` (0/1, default 1)
  - Enables pulsing when items are picked up.
- `cl_crosshairSize` (pixels, default 32)
  - Base crosshair size in pixel units before UI scaling. Also scales hit marker
    sizing via `cl_crosshairSize / 32`.

## Hit Styles
- 0 - Off.
- 1 - Colorize crosshair based on damage dealt.
- 2 - Colorize to `cl_crosshairHitColor`.
- 3 - Pulse the crosshair (large pulse).
- 4 - Damage color + large pulse.
- 5 - Hit color + large pulse.
- 6 - Pulse the crosshair (small pulse).
- 7 - Damage color + small pulse.
- 8 - Hit color + small pulse.

## Damage Colorization
Damage-based colorization uses a green-to-red gradient:
- `t = clamp(damage / 100, 0, 1)`
- `color = (255 * t, 255 * (1 - t), 0)`

## Pulse Behavior
- Item pickup pulse duration is 200ms when `cl_crosshairPulse` is enabled.
- Hit pulses use `cl_crosshairHitTime` for their duration (default 200ms).
- Large pulse scales to 1.5x, small pulse scales to 1.25x.
- When pickup and hit pulses overlap, the larger scale is applied.

## Quake Live Crosshair Palette (1-26)
Index | Name | Hex (RRGGBBAA)
1 | Red | 0xFF0000FF
2 | Orange-Red | 0xFF4000FF
3 | Dark Orange | 0xFF8000FF
4 | Orange | 0xFFC000FF
5 | Yellow | 0xFFFF00FF
6 | Green-Yellow | 0xC0FF00FF
7 | Chartreuse | 0x80FF00FF
8 | Green 1 | 0x40FF00FF
9 | Green 2 | 0x00FF00FF
10 | Spring Green 1 | 0x00FF40FF
11 | Spring Green 2 | 0x00FF80FF
12 | Green-Cyan | 0x00FFC0FF
13 | Cyan | 0x00FFFFFF
14 | Deep Sky Blue | 0x00C0FFFF
15 | Azure | 0x0080FFFF
16 | Cobalt | 0x0040FFFF
17 | Blue | 0x0000FFFF
18 | Electric Ultramarine | 0x4000FFFF
19 | Electric Purple | 0x8000FFFF
20 | Lilac | 0xC000FFFF
21 | Magenta 1 | 0xFF00FFFF
22 | Magenta 2 | 0xFF00C0FF
23 | Bright Pink | 0xFF0080FF
24 | Folly | 0xFF0040FF
25 | White | 0xFFFFFFFF
26 | Medium Grey | 0x808080FF
