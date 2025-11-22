# UI Renderer Sample Scenes

These sample scenes exercise every primitive described in `doc/ui_drawing_interface.md` across all interaction states and a few DPI/layout scale combinations. Use them as reference data when validating rendering changes or authoring automated fixtures.

## Coverage highlights
- **Primitives**: `fill_rect`, `stroke_rect`, `icon`, and `text` appear in every scene so stateful styling can be compared side-by-side.
- **States**: `normal`, `hover`, `active`, and `disabled` assignments are distributed across commands to surface per-state palette or typography overrides.
- **Scaling**: Scenes step through 1.0×, 1.5×, and 2.0× DPI values, with the last scene also applying a 1.25× layout zoom to stress combined scaling.

## Manual validation checklist
1. Load a scene and verify that hover/active/disabled variants respect the expected palette keys.
2. Check that icon and text rendering stay crisp and aligned after DPI/layout scaling.
3. Confirm that stroke widths and corner radii remain visually balanced against the fills at each scale.
4. Toggle theme palettes or fonts to ensure palette/tokens resolve consistently for every state.

## Automated checks
The accompanying `test_ui_renderer_scenes.py` test ensures the JSON stays exhaustive. If you add new primitives or states, update both the sample scenes and the assertions to keep coverage aligned.
