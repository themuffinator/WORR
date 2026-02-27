# Item Colorize Rendering (2026-01-28)

## Summary
Added a client-side item colorize path that applies a transparent color overlay to item world models while preserving the original skin's detail. Color targets come from a new per-item RGB field in the item list and are delivered to clients via a new configstring range. A new client cvar controls the overlay strength.

## New Data and Configstrings
- `Item::colorize` (sgame): per-item RGB color target, default `COLOR_WHITE`.
- `CS_ITEM_COLORS`: new configstring range, one entry per item (follows `CS_FLAGSTATUS`).
  - Format: "<model_index> <r> <g> <b>"
  - `model_index` is the index of the item's `worldModel` in the `CS_MODELS` list.
- `SetItemNames()` now publishes `CS_ITEM_COLORS + item_id` using the item's `worldModel` and `colorize`.

## Client Parsing
- `cl.item_color_by_model[MAX_MODELS]` stores color by model index.
- `cl.item_color_model_index[MAX_ITEMS]` tracks the model index assigned to each item.
- `CL_ParseItemColorConfigstring()` parses the per-item configstring and updates the model index mapping.
- `cl.csr.itemcolors < 0` (legacy protocols) disables parsing and ignores this data.

## Rendering Pipeline
- New renderer flag: `RF_ITEM_COLORIZE`.
 

### Entity Assembly
- For non-player entities, if `cl_colorize_items` is enabled and a model-index color exists:
  - `ent.flags |= RF_ITEM_COLORIZE`
  - `ent.rgba` is set to the item RGB (alpha stores overlay strength)

### OpenGL
- `setup_color()` applies an overlay factor to lighting when `RF_ITEM_COLORIZE` is set.

### Vulkan (vkpt)
- `ModelInstance.colorize` (vec4) carries the item tint (RGB) and overlay strength in `.a`.
- `get_material()` applies the tint as an overlay factor to the base color.

## Cvar
- `cl_colorize_items` (default `0`, archived)
  - `0`: no changes to item rendering.
  - `0.0-1.0`: strength of the color overlay (0.5 is a mid-strength tint).

## Notes / Limits
- Colorization is keyed by model index; items sharing a world model will share the same tint.
- Default per-item color is white, so enabling the cvar without per-item overrides yields no visible change.
- `CS_FLAGSTATUS` is now part of the shared configstring list so indices remain aligned across engine/game headers.
