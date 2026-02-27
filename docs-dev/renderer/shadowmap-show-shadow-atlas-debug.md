# Shadow Atlas Debug Overlay (`gl_show_shadow_atlas`)

## Summary

`gl_show_shadow_atlas` now renders a Quake II Rerelease-style occupancy panel instead of independent layer thumbnails.

- Single black square atlas panel in top-left.
- Slot packing visualized from bottom-right to left, then upward.
- Per-item borders show light class/visibility state.
- Dynamic/effect shadow lights are drawn as smaller (fractional) tiles.

## Cvar

`gl_show_shadow_atlas`:

- `0`: disabled.
- `1`: occupancy panel with active slot contents and overflow markers.
- `2`: same as `1`, plus dynamic slot mini-preview content.

## Visual Encoding

Border colors are derived from actual renderer-side light metadata:

- `green`: static/entity shadow light (`DL_SHADOW_LIGHT`) visible via PVS2-only path.
- `blue`: static/entity shadow light visible in base PVS (also visible in PVS2).
- `teal`: dynamic/effect shadow light (`DL_SHADOW_DYNAMIC`) currently assigned a shadow slot.
- `red`: dynamic/effect shadow light eligible/visible but not assigned a slot this frame (overflow/evicted).

## Layout Model

Current GL shadowmaps are implemented as `GL_TEXTURE_2D_ARRAY` layers, not a native 2D packed atlas.
To provide atlas-style observability, the debug panel uses a virtual layout:

1. Shadow slots are arranged in square pages.
2. Each slot is shown as a 4x4 block.
3. Face occupancy inside a slot fills from bottom-right leftward (then upward).
4. Dynamic/effect lights use a half-size marker tile in their slot block.

This preserves current runtime storage while exposing a compact atlas-like occupancy view.

## Implementation Notes

- File: `src/rend_gl/main.c`
- New per-light visibility tracking captures PVS and PVS2 state during shadow light selection:
  - `gl_shadow_light_vis[]`
  - `GL_ShadowmapLightVisibility(...)`
- Debug rendering copies selected array layers into a transient `GL_TEXTURE_2D` debug texture for tile previews.
- Overlay primitives use renderer 2D draw path (`R_DrawFill32`) and border helpers.

## Atlas Management Changes

A small slot-management policy change was applied to better match expected behavior under pressure:

- Slot selection now prefers static/entity shadow lights over dynamic/effect lights when competing for atlas capacity.
- Existing hysteresis reuse remains, but only within the same light class priority (static first, then dynamic).

This keeps persistent/static shadowlights from being displaced too aggressively by transient effects.

## Validation

1. Enable shadowmaps: `gl_shadowmaps 1`.
2. Enable panel: `gl_show_shadow_atlas 1`.
3. Confirm panel appears as a black square in top-left and fills from bottom-right.
4. Trigger both static shadow lights and transient effect lights.
5. Verify border colors and dynamic mini-tile behavior.
6. Create slot pressure (many lights) and verify red overflow markers appear and static lights remain preferred.
