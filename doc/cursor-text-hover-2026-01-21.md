Text cursor hover (2026-01-21)

Summary
- Switch the menu cursor to /gfx/cursor-text.png while hovering text input widgets.

Details
- Added cursor-text loading for legacy and cgame UI and swapped the cursor draw to use the text cursor when the mouse is over a field/combobox input.
- Kept the default cursor hotspot centered and used a top-left hotspot for /gfx/cursor-text.png to match the asset alignment.
