# bobskip client cvar

## Overview

Adds a client-side `bobskip` cvar that disables view bobbing and related feedback
effects. When enabled, the client ignores server-supplied view offsets and kick
angles, clears the damage blend overlay, and removes view-weapon bob/lag from the
rendered gun and player beams.

## Cvar

- Name: `bobskip`
- Type: client (userinfo), archived
- Default: `0`
- Values:
  - `0`: default behavior (view bobbing and damage feedback active)
  - `1`: disable view bobbing, view kicks, and damage blend overlay

## Behavior changes

- View kick angles from weapon fire, damage, fall, and movement are not applied.
- View offsets from the playerstate (bobbing, kick origin, fall offsets) are ignored.
- Damage blend overlay is cleared after interpolation.
- View-weapon model ignores gun bob/lag offsets and angles.
- Player beam origins skip gun offsets to remain stable with the view.

## Implementation notes

- New userinfo cvar wired in `src/client/main.c`.
- Rendering changes in `src/client/entities.c` and `src/client/tent.c`.
