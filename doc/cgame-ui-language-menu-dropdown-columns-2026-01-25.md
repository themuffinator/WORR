# Cgame UI Language Menu + Dropdown Overhaul (2026-01-25)

## Summary
- Added a Language settings page and Options entry, wiring `loc_language` into a dedicated dropdown.
- Rebuilt dropdown widgets into real list popovers with scrolling, selection, and cancel behavior.
- Introduced label/value column alignment for setting widgets and refreshed widget visuals with framed controls and focus states.
- Updated menu/list scrollbars to match the new UI framing.

## Dropdown list widget
- Dropdowns now open a list panel anchored to the control, sized by label width and available screen space.
- Supports keyboard navigation (up/down/page/home/end), mouse wheel scrolling, click selection, and escape-to-cancel.
- Adds a scrollbar when entries exceed 8 visible rows and auto-positions above or below the control based on space.

## Column alignment + widget visuals
- Menu layout computes a label/value column split for setting widgets (slider/spin/dropdown/switch/field/combo/keybind).
- Labels clip within the left column; control widgets draw inside framed boxes with consistent padding.
- Focused widgets render brighter borders and row accents; toggles, sliders, and fields now use box primitives instead of text-only cues.

## Language settings menu
- New `language` menu page with `loc_language` dropdown containing auto plus all shipped localization files.
- Options menu includes a Language entry to access the page.
