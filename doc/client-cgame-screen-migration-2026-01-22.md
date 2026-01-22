# Client -> cgame screen migration (2026-01-22)

## Summary
- Routed crosshair, hit marker, damage indicator, and POI rendering through cgame when exports are available.
- Added POI state and projection/rendering to cgame using view parameters and screen metrics.
- Kept legacy client paths as fallback when cgame lacks the new exports.

## Implementation details
- `src/client/screen.cpp` now calls `cgame->DrawCrosshair` in `SCR_Draw2D` and forwards pickup pulse, damage display, and POI updates to cgame when available.
- `src/client/main.cpp` forwards hit marker notifications to cgame while preserving local hit marker state for legacy drawing and sound gating.
- `src/game/cgame/cg_draw.cpp` now tracks POIs per split, resolves image names, projects world positions into HUD space, applies edge scaling and hide-on-aim alpha, and draws via `SCR_DrawColorPic`.
- `src/game/cgame/cg_main.cpp` exports `AddPOI`/`RemovePOI` so the client can hand off POI updates.

## Compatibility notes
- Client-side crosshair/damage/POI code remains and is used when cgame does not export `DrawCrosshair`.
- Hit marker sound playback remains in the client, aligned with the existing per-frame gating behavior.

## Testing
- Not run (local build/behavior verification needed).
