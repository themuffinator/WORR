Cgame UI Widget Modernization (2026-01-25)

Overview
- Dropdown lists now render as overlays so they always draw on top, accept mouse clicks, and include a functional scrollbar when the list is longer than the visible threshold.
- Image-based dropdowns (crosshair selection) now mirror standard dropdown behavior with a tiled grid overlay, hover borders, selection highlight, and list scrolling.
- Text inputs support selection, cursor alignment, insert/overstrike rendering, and expected navigation/editing across UI fields, console input, and messagemode input.
- Slider widgets gain discrete step marks, a 26-color rainbow track for crosshair colors, and hover animations that scale/lighten the thumb while tinting it to the selected color.
- New heading widgets provide bold section labels with divider lines for menus like key bindings.
- Cursor tooltips draw near the mouse using widget status text, replacing the old bottom-only guidance with a modern, contextual callout.
- 3D preview rendering (RDF_NOWORLDMODEL) now clears its color buffer per frame to prevent accumulation artifacts in model previews, with VKPT accumulation reset in menu renders.

UI Widgets
- DropdownWidget and ImageSpinWidget draw list overlays after main menu widgets; overlay hit-testing and mouse wheel handling keep list interaction reliable.
- ImageSpinWidget uses grid layout based on tile size and menu width; hover items add a brighter border, selected items use active coloring.
- HeadingWidget renders a larger, bolded label (double-draw) and a subtle horizontal rule to separate sections.
- Tooltip panel uses primitives: shadow, fill, and border with padding; text wraps to a reasonable width and avoids the hint bar.

Input Fields
- inputField_t tracks selection anchor and selecting state.
- IF_KeyEvent supports shift-selection, Ctrl+A/C/X/V, and selection deletion with backspace/delete.
- IF_Draw renders selection highlight and uses font metrics to align the cursor.
- UI FieldWidget/ComboWidget, console input, and messagemode input now share the same selection/cursor logic and insert/overstrike visuals.

Sliders
- Tick marks are drawn for each step (capped for performance).
- Rainbow sliders are enabled for crosshair color cvars (1-26), with a color-matched thumb and hover animation.

Renderer
- OpenGL: GL_Setup3D clears color + depth for RDF_NOWORLDMODEL with scissor restricted to the view rect to avoid 3D overdraw artifacts.
- VKPT: temporal accumulation is reset when rendering RDF_NOWORLDMODEL previews to prevent persistence/ghosting.

Files Touched
- src/game/cgame/ui/ui_widgets.cpp
- src/game/cgame/ui/ui_internal.h
- src/game/cgame/ui/ui_menu.cpp
- src/game/cgame/ui/ui_json.cpp
- src/game/cgame/ui/worr.json
- src/common/field.c
- inc/common/field.h
- src/client/console.cpp
- src/client/screen.cpp
- src/rend_gl/state.c
- src/rend_vk/vkpt/main.c
