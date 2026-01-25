# Weapon Bar Refactor and cl_weapon_bar

## Summary
The former "carousel" weapon bar UI has been split out of `wheel.c` into a
dedicated weapon bar module and renamed throughout to "weapon bar". This
adds a Quake Live-style `cl_weapon_bar` cvar with multiple display modes,
including static left/right/center positions and two timed variants.

## New cvar: cl_weapon_bar
Default: `5` (Quake II Rerelease timed weapon bar).

Values and behavior:
- `0` Disabled: no weapon bar drawn, but `cl_weapnext`/`cl_weapprev` still cycle
  weapons immediately.
- `1` Static left: always visible vertical bar on the left side.
- `2` Static right: always visible vertical bar on the right side.
- `3` Static center: always visible horizontal bar centered near the bottom.
- `4` Timed Quake 3 legacy: horizontal bar appears temporarily after
  weapon cycling (simple icon highlight, no weapon name or counts).
- `5` Timed Quake II Rerelease: existing Q2R carousel behavior, including
  delayed switch until timeout/attack and weapon name display.

Notes:
- Modes `0-4` perform immediate weapon switching via `use_index_only`.
- Mode `5` preserves the Q2R timed-selection behavior.

## Layout and tuning
The weapon bar module still uses the legacy `wc_*` cvars (names unchanged)
for the Q2R timed mode:
- `wc_screen_frac_y` (vertical placement)
- `wc_timeout` (selection timeout)
- `wc_lock_time` (attack lockout after switch)
- `wc_ammo_scale` (ammo count scale)

Static and Q3-timed layouts use fixed positioning constants for left/right
margin and center placement, matching the new modes without adding extra
cvars.

## Code changes
Key refactor points:
- New module: `src/client/weapon_bar.c` (formerly carousel logic).
- State renamed: `client_state_t.carousel` -> `client_state_t.weapon_bar`.
- UI handle renamed: `scr.carousel_selected` -> `scr.weapon_bar_selected`.
- `wheel.c` no longer owns weapon bar logic; it now delegates weapon cycling
  to `CL_WeaponBar_Cycle`.
- Rendering/input call sites updated to `CL_WeaponBar_*` functions.
- Build updated to compile `src/client/weapon_bar.c`.
