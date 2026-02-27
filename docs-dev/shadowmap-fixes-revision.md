# Shadowmap Fixes Revision

## Summary
- Restored MD5 shadow pass shader selection and attribute bindings so skeletal models cast shadows again.
- Ensured mesh and skeleton uniform data is uploaded during shadow passes.

## Details
### Shadow Pass State/Attributes (Alias + MD5)
- Shadow pass now preserves `GLS_MESH_MD5` in the state bits to select the skeletal shader.
- When using VA_NONE (GPU paths), the correct attribute masks are set:
  - MD5: `GLA_MESH_LERP | GLA_NORMAL`
  - MD2: `GLA_MESH_STATIC` or `GLA_MESH_LERP`
- VA-based meshes still use `GLA_VERTEX`.

### Uniform Updates
- Marked mesh uniforms dirty after setting scale/translate/backlerp so shadow passes upload the correct data.
- Marked MD5 weight/joint offsets dirty when using buffer textures, ensuring the skeleton shader has correct indices.

## Files Touched
- `src/rend_gl/mesh.c`
