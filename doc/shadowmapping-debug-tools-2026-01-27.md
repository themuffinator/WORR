# Shadowmapping/Dlight Debug Toolkit - 2026-01-27

## Goals
- Provide visual overlays to inspect dlight influence, shadowmap selection, and caster coverage.
- Provide console diagnostics for shadowmap selection and cache state.
- Keep the tools lightweight and opt-in for troubleshooting.

## New Visual Debugging (OpenGL)

### `gl_shadow_draw_debug` (bitmask)
Enable live, per-frame debug geometry and labels:
- `1` (`SHADOWDBG_DRAW_ALL_LIGHTS`): draw influence spheres for all dlights.
- `2` (`SHADOWDBG_DRAW_SELECTED`): draw influence spheres for selected shadowmap lights.
- `4` (`SHADOWDBG_DRAW_CONES`): draw spot light cones (axis + pyramid outline).
- `8` (`SHADOWDBG_DRAW_CASTERS`): draw shadow-caster bounds (spheres) for casters inside selected lights.
- `16` (`SHADOWDBG_DRAW_CSM`): draw sun cascade bounds.
- `32` (`SHADOWDBG_DRAW_TEXT`): draw per-light text labels (slot, type, radius, cache state, PCSS).

Notes:
- Use `gl_shadow_debug_light` to filter to a single dlight index.
- Use `gl_shadow_debug_freeze 1` to lock the current shadowmap light selection.

## Console Debugging

### `gl_shadow_dump [selected | <index>]`
Print a snapshot of dlights and shadowmap selection:
- No args: prints all dlights.
- `selected`: prints only shadowmap-selected lights.
- `<index>`: prints a single dlight.

### `gl_shadow_debug_log` + `gl_shadow_debug_log_ms`
Rate-limited logging while running:
- `gl_shadow_debug_log 0`: off (default).
- `gl_shadow_debug_log 1`: summary line only.
- `gl_shadow_debug_log 2`: selected lights.
- `gl_shadow_debug_log 3`: all lights.
- `gl_shadow_debug_log_ms` controls the minimum time between logs (default 250ms).

## Affected Files
- `src/rend_gl/main.c`
- `src/rend_gl/gl.h`

## Example Sessions
- Show selected shadowmap lights with labels:
  - `gl_shadow_draw_debug 34` (2 + 32)
- Show all dlight spheres and spot cones:
  - `gl_shadow_draw_debug 5` (1 + 4)
- Dump selected lights once:
  - `gl_shadow_dump selected`
- Log selected lights while moving:
  - `gl_shadow_debug_log 2` and `gl_shadow_debug_log_ms 500`
