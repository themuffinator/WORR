# AGENTS cvar prefixes (2026-01-23)

## Overview
- Added a consolidated reference for common cvar prefixes in `AGENTS.md` to standardize naming and reduce cross-subsystem drift.
- Captures both current WORR usage and legacy Q2-compatible prefixes.

## Details
- Listed engine, game, renderer, and platform prefixes: `cg_`, `sg_`, `g_`, `cl_`, `sv_`, `ui_`, `r_`, `gl_`, `vk_`, `pt_`, `vid_`, `con_`, `win_`, `com_`, `scr_`.
- Added frequently used subsystem prefixes for audio, input, network, filesystem, system, localization, MVD, and weapon wheel: `s_`, `al_`, `in_`, `m_`, `net_`, `fs_`, `sys_`, `loc_`, `mvd_`, `ww_`.
- Noted preference for `sg_` on new sgame cvars while retaining legacy `g_` compatibility.

## Compatibility
- Documentation-only change; no runtime impact.

## Testing
- Not run (documentation change).
