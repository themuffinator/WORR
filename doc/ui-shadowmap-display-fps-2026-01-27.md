# UI and Shadowmap Fixes (2026-01-27)

## Summary
- Stabilized shadowmap passes by preventing visframe collisions and forcing a fresh PVS build before the main view.
- Reduced the FPS counter draw scale without changing the global HUD scale.
- Updated image-based selection widgets to support numeric cvar values and applied it to crosshair selection.
- Expanded video menu options to expose fullscreen type and display selection.

## Details
### Shadowmap visframe handling
- Shadowmap visibility marking now keeps `glr.visframe` advanced after shadow passes.
- The main view always rebuilds PVS after shadow rendering to avoid stale or overlapping visibility flags.
- This prevents shadowmap passes from polluting main view visibility and avoids scrambled world output.

### FPS counter sizing
- The FPS counter now renders at half scale by temporarily overriding the render scale for its draw call.
- Coordinates are adjusted to keep the FPS counter anchored in the same screen position.

### Crosshair imagevalues numeric mapping
- Image spin widgets can map a filename prefix to a numeric cvar value when requested.
- Crosshair selection now reads and writes integer `crosshair` values (e.g., `1`) while still displaying the matching image.
- Legacy string values are parsed when possible so existing configs still select the correct crosshair.

### Video menu display options
- Added fullscreen type selection (`r_fullscreen_exclusive`) and display targeting (`r_display`) to the video menu.
- Display input includes guidance for auto selection vs explicit display naming.

## Files
- `src/rend_gl/main.c`
- `src/game/cgame/cg_draw.cpp`
- `src/game/cgame/ui/ui_internal.h`
- `src/game/cgame/ui/ui_widgets.cpp`
- `src/game/cgame/ui/ui_json.cpp`
- `src/game/cgame/ui/worr.json`
