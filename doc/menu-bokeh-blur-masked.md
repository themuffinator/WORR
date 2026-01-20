# Masked Menu Bokeh Blur

## Overview
- Restricts the menu bokeh blur to the transparent menu background region instead of the full view.
- Raises the default menu blur intensity to make the effect more apparent out of the box.
- Ensures map list menus populate even when the map pool has not been loaded yet.

## Menu Blur Masking
- UI menus now compute a blur rectangle based on the drawn background region (full width, menuTop..menuBottom).
- The rectangle is only sent when the menu background is transparent; opaque menus clear the blur mask.
- UI coordinates are converted back to base virtual coordinates before being passed to the client.
- `MenuSystem::ForceOff` and `MenuSystem::Draw` clear the blur mask when no menu is active to avoid stale state.

## Client/Renderer Wiring
- `cgame_ui_import_t` gains `CL_SetMenuBlurRect`, and the UI import/export API bumps to v3.
- `client_state_t` stores `menu_blur_active` and `menu_blur_rect`, driven by the UI import call.
- `refdef_t` gains `dof_rect_enabled` and `dof_rect`; `V_RenderView` enables the mask only when menu blur is active.
- `GL_DrawDof` intersects the view rect with `dof_rect` when enabled, so DOF only composites the masked region.

## Blur Strength
- `cl_menu_bokeh_blur` default increases from `0.6` to `0.85` (still clamped to `[0..1]`).

## Map List Menus
- The MyMap and Callvote map lists now call `LoadMapPool` when the pool is empty before building entries.
- If loading fails or the pool is still empty, the list continues to show the "No maps available" placeholder.
