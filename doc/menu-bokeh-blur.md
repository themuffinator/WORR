# Menu Bokeh Blur

## Overview
- Adds a depth-of-field "bokeh" blur behind menus when the cgame UI has menu input focus.
- Uses the existing `refdef_t::dof_strength` pipeline so the effect stacks with other DOF drivers.

## Behavior
- When `KEY_MENU` is active, the menu blur blend ramps toward a target strength from `cl_menu_bokeh_blur`.
- The blend uses the same frame-time smoothing as the wheel/layout DOF transition.
- The final DOF strength is the max of: wheel ramp, layout ramp, and menu blur ramp.
- Menu blur is reset to 0 whenever the client frame is invalid.

## Cvar
- `cl_menu_bokeh_blur` (default `0.85`, flags `CVAR_ARCHIVE`) controls the blur strength; `0` disables it.

## Notes
- Requires `r_dof` to be enabled; respects `r_dof_blur_range` and `r_dof_focus_distance` auto values.
- Only applies when the world is actively rendered, so front-end menus without a valid frame remain unaffected.
- Masked menu blur behavior is documented in `doc/menu-bokeh-blur-masked.md`.
