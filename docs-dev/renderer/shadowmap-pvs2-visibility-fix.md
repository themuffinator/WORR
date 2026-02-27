# Shadowmap PVS2 Visibility and MD2 Caster Bounds Fix

## Summary

This change fixes three shadowmapping issues:

1. Server-side entity culling dropped shadow-relevant entities that were outside
   strict PVS/PHS handling used by the client frame builder.
2. Renderer-side shadow light selection used the main view `visframe` test, so
   lights outside the camera PVS were discarded even when they could affect
   visible surfaces.
3. MD2 alias caster bounds were evaluated from origin-centered radius data
   instead of translated frame bounds, producing incorrect shadow caster
   inclusion and projection behavior.

## Affected Subsystems

- Server visibility and entity replication:
  - `src/server/entities.c`
  - `SV_BuildClientFrame`
- OpenGL renderer shadow light selection and caster bounds:
  - `src/rend_gl/main.c`
  - `GL_ShadowmapLightVisible`
  - `GL_EntityShadowOrigin`
  - `GL_EntityShadowRadius`

## Server Culling Changes

### PVS2 mask setup

`SV_BuildClientFrame` now computes an additional mask:

- `BSP_ClusterVis(client->cm->cache, &clientpvs2, clientcluster, DVIS_PVS2);`

`BSP_ClusterVis` already falls back to PVS when PVS2 data is absent.

### Shadow-aware visibility selection

Entity visibility now uses:

- `clientphs` for beam/sound culling (unchanged behavior intent).
- `clientpvs2` for shadow-affecting entities:
  - `ent->s.renderfx & RF_CASTSHADOW`
  - or `ent->s.modelindex && !(ent->s.renderfx & RF_NOSHADOW)`
- `clientpvs` for non-shadow-affecting entities.

For sound attenuation checks where a model may still be sent, the visibility
recheck is also shadow-aware (`clientpvs2` for shadow-affecting entities).

## Renderer Shadow-Light Selection Changes

### Cached PVS2 mask per view cluster pair

The GL renderer now keeps a cached PVS2 mask for shadow light visibility tests:

- cache state:
  - `gl_shadow_pvs2_mask`
  - `gl_shadow_pvs2_cluster1`
  - `gl_shadow_pvs2_cluster2`
  - `gl_shadow_pvs2_bsp`
- update helper:
  - `GL_UpdateShadowPvs2Mask(const bsp_t *bsp)`

Behavior:

1. Rebuild only when BSP changes or `(viewcluster1, viewcluster2)` changes.
2. Build from `DVIS_PVS2` for `viewcluster1`.
3. If `viewcluster2 != viewcluster1`, OR the second PVS2 row into the mask.

### Light visibility test update

`GL_ShadowmapLightVisible` now uses:

- `Q_IsBitSet(gl_shadow_pvs2_mask.b, leaf->cluster)`

instead of:

- `leaf->visframe == glr.visframe`

Area-bit filtering (`glr.fd.areabits`) is unchanged.

## MD2 Alias Shadow-Caster Bounds Changes

### New alias bounds helper

`GL_GetAliasShadowBounds(const entity_t *ent, const model_t *model, vec3_t mins, vec3_t maxs)`

The helper:

1. Resolves `frame` and `oldframe` with `glr.fd.extended` handling.
2. Unions `model->frames[frame].bounds` and `model->frames[oldframe].bounds`.
3. Returns conservative alias bounds for shadow tests.

### Shadow origin update

`GL_EntityShadowOrigin` now uses alias bounds center for non-inline alias
models:

- `center = (mins + maxs) * 0.5`
- applies absolute per-axis scale
- rotates by entity angles if needed
- translates by `ent->origin`

### Shadow radius update

`GL_EntityShadowRadius` now computes alias radius from scaled half extents:

- `half_extents = (maxs - mins) * 0.5`
- apply `abs(scale.xyz)`
- `radius = length(half_extents)`

This replaces the previous alias path that used frame radius centered at origin
and could misrepresent translated MD2 bounds.

## Multiplayer Implications

This change is intentionally default-on in all modes.

Tradeoff:

- Pros:
  - Fewer missing shadows and less shadow/light pop from conservative culling.
- Cons:
  - More entities can be sent/considered versus strict PVS/PHS-only behavior.
  - In multiplayer, clients may receive some extra shadow-relevant entity
    presence information versus stricter visibility-only replication.

No protocol-level changes were introduced.

## Validation Scenarios

1. `base3` start area:
   - Confirm guard shadow/light behavior remains visible where strict PVS can
     otherwise miss shadow influence.
2. Shadow-light debug:
   - `gl_shadow_draw_debug 1`
   - Verify shadow-light counts include off-PVS lights that still influence
     visible areas.
3. MD2 caster checks:
   - Spawn MD2 monsters with translated frames near shadow lights.
   - Verify reduced shadow pop and stable caster inclusion while moving.
4. Regression:
   - Confirm normal world PVS culling/rendering remains unchanged outside
     shadow-light selection and shadow-related entity visibility expansion.

