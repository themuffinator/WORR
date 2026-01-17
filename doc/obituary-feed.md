# Obituary Feed (CGame)

## Overview
The cgame HUD now renders a kill feed in the top-left of the player HUD. Entries are built from server obituary prints and rendered as:
- killer icon victim for normal kills
- icon victim for suicides and world deaths

The feed keeps up to four lines, expires entries after 3 seconds, and fades them out over the final 200 ms.

## Event Detection
Obituaries are detected in cgame by matching incoming print strings against localized obituary templates. Each template is localized with placeholder tokens, then the message is parsed by splitting on the resulting prefix/between/suffix segments to extract the victim and (if present) killer names.

This avoids hardcoding English text while still allowing icon selection.

## Template Mapping
The following localization keys are recognized and mapped to icons or fallback labels:
- World/self deaths: `$g_mod_generic_*`, `$g_mod_self_*`, `$g_mod_generic_died`
- Player kills: `$g_mod_kill_*`

Weapon-related keys map to existing item/weapon icons (for example `w_shotgun`, `w_rlauncher`, `w_bfg`). Non-weapon or unsupported methods fall back to short labels such as `FALL`, `LAVA`, or `TELEFRAG`.

## Rendering Rules
- Max entries: 4
- Lifespan: 3000 ms
- Fade: last 200 ms (alpha ramp down)
- Placement: top-left of the HUD, offset below notify lines (`scr_maxlines`)
- Icons are drawn at the current font line height; if an icon is missing, the label text is drawn instead.
