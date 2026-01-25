# UI slider/dropdown mouse fixes + Hor+ FOV enforcement (2026-01-25)

## Summary
- Added mouse dragging/clicking support for slider widgets.
- Reworked the graphical dropdown preview/layout and made dropdown lists fully opaque.
- Forced Hor+ FOV behavior and removed the menu toggle for fov scaling.

## Changes
- Slider widgets now snap to the nearest step on mouse click and stay in sync while dragging.
- Graphical dropdowns (image values) render their header and preview areas separately so the selected image scales into the reserved preview space.
- Image dropdown lists clamp to a sane width, cap columns at three for compact tiles, and fall back to single-column rows for wide items.
- Dropdown list panels, rows, and scrollbars now render fully opaque to avoid background bleed.
- Hor+ FOV adjustment is always applied; `cl_adjustfov` is read-only and no longer exposed in menus.

## Files
- `src/game/cgame/ui/ui_internal.h`
- `src/game/cgame/ui/ui_widgets.cpp`
- `src/game/cgame/ui/ui_menu.cpp`
- `src/game/cgame/ui/worr.json`
- `src/client/ui/worr.menu`
- `src/client/view.cpp`
- `src/rend_gl/mesh.c`
- `src/rend_vk/vkpt/matrix.c`

## Validation
- Rebuild the client and open the crosshair menu to confirm image dropdown layout/scrolling.
- Verify slider widgets respond to click and drag, and Hor+ FOV remains active regardless of config.
