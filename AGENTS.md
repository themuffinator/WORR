# AGENTS

## Purpose
Advance Q2REPRO with the full functionality of Quake II Rerelease under the WORR banner.

## Rules
- Document significant changes in detail in a new, appropriate document.
- Backwards compatibility is not a requirement, only a nice-to-have.
- Use idTech3 (Quake 3 Arena) codebases as refactoring inspiration (Quake3e: `E:\_SOURCE\_CODE\Quake3e-master`, baseq3a: `E:\_SOURCE\_CODE\baseq3a-master`) to replicate its advantages as WORR evolves.
- Treat `q2proto/` as read-only unless explicitly prompted otherwise; creating/developing a new WORR protocol is allowed when needed, but must preserve compatibility with legacy Q2 servers and demos.

## Common cvar prefixes
- `cg_`: client game (`cgame`).
- `sg_`: server game (`sgame`); prefer for new sgame cvars.
- `g_`: legacy Q2 sgame cvars; keep for compatibility.
- `cl_`: client engine (`client`).
- `sv_`: server engine (`server`).
- `ui_`: UI in client game (`cgame`).
- `r_`: shared renderer (`renderer`).
- `gl_`: OpenGL renderer exclusive (`rend_gl`).
- `vk_`: Vulkan renderer exclusive (`rend_vk`).
- `pt_`: Vulkan path tracing (vkpt) renderer cvars (`rend_vk`).
- `vid_`: generic renderer cvar, ideally replace with `r_`.
- `con_`: console cvar (`client/console.cpp`).
- `win_`: Windows-only.
- `com_`: common engine cvar (`common`).
- `scr_`: legacy client screen cvar, replace with `cl_` in future (`client/screen.cpp`).
- `s_`: audio system cvars.
- `al_`: OpenAL-specific audio cvars.
- `in_`: input system cvars.
- `m_`: mouse input cvars.
- `net_`: networking cvars.
- `fs_`: filesystem cvars.
- `sys_`: system/platform cvars.
- `loc_`: localization cvars.
- `mvd_`: server MVD/streaming/demo cvars.
- `ww_`: weapon wheel UI cvars.

## Cvar naming conventions
- Use lowercase `snake_case` (for example `cg_draw_graphical_obits`).
- Use `draw` in the cvar name when it enables rendering a UI element.
