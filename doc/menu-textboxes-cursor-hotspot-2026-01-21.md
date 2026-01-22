Menu text boxes and cursor hotspot (2026-01-21)

Summary
- Align the menu cursor hotspot so the pointer coordinates match the top-left of the cursor image.
- Render text input fields as modern text boxes in both legacy and cgame UI paths.

Details
- Updated legacy and cgame menu cursor draws to use a top-left hotspot for the default cursor image.
- Added a shared textbox-style background + border for text input widgets (fields and comboboxes) in cgame UI.
- Updated legacy menu field rendering to draw a similar textbox-style frame instead of a flat fill.
