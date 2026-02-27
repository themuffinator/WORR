# Shadowmap Fixes Implementation

## Summary
- Adjusted shadow pass mesh rendering to use the correct vertex attributes for GPU-lerped alias models.
- Ensured the shadow pass uses shadowmap-only state bits (plus MD2/lerp where required) and reloads the entity matrix.
- Reinforced identity model matrix use for world geometry in the shadow pass.

## Details
### Entity Shadow Pass (Alias Models)
- Shadow pass state now masks to `GLS_MESH_MD2`/`GLS_MESH_LERP` only, avoiding unrelated mesh flags.
- When `VA_NONE` is active (GPU lerp path), the pass explicitly enables `GLA_MESH_STATIC` or `GLA_MESH_LERP` so the shader receives mesh attributes (`a_new_pos`/`a_old_pos`).
- When using VA-based meshes, the pass enables only `GLA_VERTEX` to keep attribute bindings minimal.
- The entity model matrix is reloaded before drawing shadow geometry to guarantee the correct transform is active.

### World Geometry Shadow Pass
- The shadow pass now reasserts the identity model matrix during world face submission, ensuring world-space positions are used consistently.

## Files Touched
- `src/rend_gl/mesh.c`
- `src/rend_gl/tess.c`
