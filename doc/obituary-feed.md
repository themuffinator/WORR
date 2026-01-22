# Obituary Feed (CGame)

## Overview
The cgame HUD renders a kill feed in the top-left of the player HUD. Entries are built from server obituary prints and rendered as:
- killer icon victim for normal kills
- icon victim for suicides and world deaths

The feed keeps up to four lines, animates line shifts smoothly, and fades entries out at the end of their lifetime.

## Event Detection
Obituaries are detected in cgame by consuming structured metadata embedded in server print strings. The metadata includes a stable event key plus the victim/killer names, so the HUD can map to icons and labels without parsing localized output.

If metadata is not present (legacy servers), cgame falls back to matching incoming print strings against localized obituary templates. Each template is localized with placeholder tokens, then the message is parsed by splitting on the resulting prefix/between/suffix segments to extract the victim and (if present) killer names.

## Template Mapping
The following localization keys are recognized and mapped to icons or fallback labels:
- World/self deaths: `$g_mod_generic_*`, `$g_mod_self_*`, `$g_mod_generic_died`
- Player kills: `$g_mod_kill_*`

Custom obituary keys cover events that lack localization entries (for example `obit_expiration`, `obit_self_plasmagun`, `obit_kill_plasmagun`, `obit_kill_thunderbolt`, `obit_self_tesla`).

Weapon-related keys map to existing item/weapon icons (for example `w_shotgun`, `w_rlauncher`, `w_bfg`). Non-weapon or unsupported methods fall back to short labels such as `FALL`, `LAVA`, or `TELEFRAG`.

## Rendering Rules
- Max entries: 4
- Lifespan: `cl_obituary_time` (default 3000 ms; values under 100 are treated as seconds)
- Fade: last `cl_obituary_fade` ms (default 200 ms; values under 100 are treated as seconds)
- Placement: top-left of the HUD, offset below notify lines (`scr_maxlines`)
- Icons are drawn at the current font line height; if an icon is missing, the label text is drawn instead.
- Line positions animate smoothly as entries shift.
