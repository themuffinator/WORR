# Comprehensive Shadow Mapping System Fix Plan

## Current Observed Issues

1. **BSP model entities (func_rotating fan) do NOT cast shadows** - light projects through
2. **MD5 player/monster shadows completely misprojected** - shadows don't align with models
3. **DL_SHADOW_DYNAMIC lights don't cast shadows** - explosions/projectiles broken (worked before for items)
4. **Static world geometry shadows work** - walls/floors cast correctly

---

## Root Cause Analysis

### Issue 1: BSP Model Entities Not Casting Shadows

**Code path**: `GL_DrawShadowEntities` → `GL_DrawEntity` → `GL_DrawBspModel`

In [world.c:392-483](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/world.c#L392-L483), `GL_DrawBspModel` renders BSP brushes using:
- `GL_BindArrays(VA_3D)` at line 441
- `GL_DrawSolidFaces()` at line 476

**Problem**: The `VA_3D` vertex format includes:
```c
[VA_3D] = {
    [VERT_ATTR_POS]    = ATTR_FLOAT(3, VERTEX_SIZE, 0),
    [VERT_ATTR_TC]     = ATTR_FLOAT(2, VERTEX_SIZE, 4),
    [VERT_ATTR_LMTC]   = ATTR_FLOAT(2, VERTEX_SIZE, 6),
    [VERT_ATTR_COLOR]  = ATTR_UBYTE(4, VERTEX_SIZE, 3),
    [VERT_ATTR_NORMAL] = ATTR_FLOAT(3, VERTEX_SIZE, 8),
}
```

But the shadowmap vertex shader at [shader.c:601-608](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/shader.c#L601-L608) expects only:
```glsl
in vec4 a_pos;  // VERT_ATTR_POS only
```

When `GL_Flush3D` is called for BSP faces in shadow pass, it sets `GLA_VERTEX` only at [tess.c:664](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/tess.c#L664). However, the issue is that BSP model faces are drawn through:
1. `GL_DrawBspModel` → `GL_AddSolidFace` → `GL_DrawSolidFaces` → `GL_DrawFace` → `GL_Flush3D`

In `GL_DrawFace` shadow pass ([tess.c:769-798](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/tess.c#L769-L798)), indices are computed but the actual vertex data comes from world buffer. The vertex buffer is bound with `VA_3D` format, but the shadowmap shader is NOT designed to read the stride of `VA_3D` data correctly.

**FIX NEEDED**: BSP model shadow rendering needs to use consistent vertex format. The shadowmap shader expects position at offset 0 with stride matching the vertex size.

---

### Issue 2: MD5 Player/Monster Shadows Misprojected

**Code path**: `GL_DrawShadowEntities` → `GL_DrawEntity` → `GL_DrawAliasModel` → `draw_alias_mesh`

In [mesh.c:700-719](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/mesh.c#L700-L719):
```c
if (glr.shadow_pass) {
    glStateBits_t state = GLS_SHADOWMAP | (meshbits & (GLS_MESH_MD2 | GLS_MESH_MD5 | GLS_MESH_LERP));
    GL_StateBits(state);
    GL_BindTexture(TMU_TEXTURE, TEXNUM_WHITE);
    if (gls.currentva == VA_NONE) {
        if (meshbits & GLS_MESH_MD5)
            GL_ArrayBits(GLA_MESH_LERP | GLA_NORMAL);
        else
            GL_ArrayBits((meshbits & GLS_MESH_LERP) ? GLA_MESH_LERP : GLA_MESH_STATIC);
    } else if (!(meshbits & GLS_MESH_MD5)) {
        GL_ArrayBits(GLA_VERTEX);
    }
    GL_LoadMatrix(glr.entmatrix, glr.viewmatrix);
    GL_LoadUniforms();
    // ... draw
}
```

**Problems Identified**:

1. **MD5 shader includes `GLS_SHADOWMAP` but also `GLS_MESH_LERP`**: This generates the MD5 skeletal vertex shader which expects joint transform uniforms (`u_joints`) and weight buffers. But the shadowmap fragment shader writes `depth = dist * u_shadow_params.w`. The MD5 vertex shader DOES compute `v_world_pos` correctly, so vertex positions should be fine IF uniform block is correctly filled.

2. **Uniform block corruption**: At [gl.h:845-848](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/gl.h#L845-L848), the mesh block overlaps with skybox matrices:
   ```c
   union {
       mat4_t          m_sky[2];
       glMeshBlock_t   mesh;
   };
   ```
   If world geometry is rendered first (setting `m_sky`), it corrupts the `mesh` union members. Then when entities render, the mesh scale/translate uniforms are garbage!

3. **CPU-tessellated meshes (non-GPU lerp)**: At [mesh.c:1270-1290](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/mesh.c#L1270-L1290), if CPU tessellation is used:
   ```c
   } else {
       GL_BindArrays(dotshading ? VA_MESH_SHADE : VA_MESH_FLAT);
       meshbits = 0;
       if (glr.shadow_pass) {
           tessfunc = newframenum == oldframenum ?
               tess_static_plain : tess_lerped_plain;
       }
   }
   ```
   The `tess_static_plain` and `tess_lerped_plain` functions compute vertex positions into `tess.vertices` buffer. These rely on global variables `oldscale`, `newscale`, `translate` which ARE set by `setup_frame_scale()` at line 1243.

4. **CRITICAL: Model matrix not loaded for GPU-lerped MD5 in shadow pass**: In `draw_alias_mesh`, `GL_LoadMatrix(glr.entmatrix, glr.viewmatrix)` is called, but if `gls.currentmodelmatrix == glr.entmatrix` already, the matrix is NOT uploaded. The model may have changed (new entity) but pointer comparison thinks they're the same since all entities use `glr.entmatrix`.

---

### Issue 3: DL_SHADOW_DYNAMIC Lights Not Working

**Code path**: `GL_SelectShadowLights` at [main.c:1700-1756](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/main.c#L1700-L1756)

Dynamic lights are only added if `gl_shadowmap_dynamic->integer` is enabled (line 1739). Check:
1. Is `gl_shadowmap_dynamic` being set to 0 somewhere?
2. Are projectile/explosion lights correctly setting `dl->shadow = DL_SHADOW_DYNAMIC`?

But the user says it "worked before for items" which suggests the selection IS happening. The issue may be:

1. **Cache invalidation**: Static light caching may interfere. At [main.c:1865-1868](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/main.c#L1865-L1868), if `is_static_shadow && cache_enabled && !dirty`, the layer is copied from cache instead of re-rendered. Dynamic lights are NOT `is_static_shadow`, so this should work.

2. **Scene hash invalidation**: The scene hash at [main.c:1796-1804](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/main.c#L1796-L1804) may cause issues if entities don't change but dynamic lights move.

---

## Definitive Fixes

### Fix 1: Force Matrix Dirty for Each Entity Shadow Draw

**File**: [mesh.c](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/mesh.c#L700-L720)

Replace `GL_LoadMatrix` with `GL_ForceMatrix` to ensure matrix is always uploaded:

```c
if (glr.shadow_pass) {
    glStateBits_t state = GLS_SHADOWMAP | (meshbits & (GLS_MESH_MD2 | GLS_MESH_MD5 | GLS_MESH_LERP));
    GL_StateBits(state);
    GL_BindTexture(TMU_TEXTURE, TEXNUM_WHITE);
    if (gls.currentva == VA_NONE) {
        if (meshbits & GLS_MESH_MD5)
            GL_ArrayBits(GLA_MESH_LERP | GLA_NORMAL);
        else
            GL_ArrayBits((meshbits & GLS_MESH_LERP) ? GLA_MESH_LERP : GLA_MESH_STATIC);
    } else if (!(meshbits & GLS_MESH_MD5)) {
        GL_ArrayBits(GLA_VERTEX);
    }
    
    // CRITICAL FIX: Always force matrix upload for each entity
    GL_ForceMatrix(glr.entmatrix, glr.viewmatrix);
    
    GL_LoadUniforms();
    GL_LockArrays(num_verts);
    qglDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT, indices);
    GL_UnlockArrays();
    return;
}
```

### Fix 2: Set Uniform Block Dirty Before Each Entity

**File**: [mesh.c](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/mesh.c) - In `GL_DrawAliasModel`, after `setup_frame_scale()`:

```c
// After setup_frame_scale(model); at line 1243
gls.u_block_dirty = true;  // Force uniform re-upload
```

This ensures the mesh scale/translate uniforms are properly uploaded even if the uniform block was partially modified by world rendering.

### Fix 3: Separate Static/Dynamic Shadow Rendering Paths

For BSP models and world geometry, create a simpler vertex path that doesn't rely on mesh uniforms:

**File**: [tess.c](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/tess.c#L769-L798) - In `GL_DrawFace` shadow pass:

```c
if (glr.shadow_pass) {
    glStateBits_t state = GLS_SHADOWMAP;
    
    // Ensure identity matrix for world/bmodel geometry
    GL_ForceMatrix(glr.ent == &gl_world ? gl_identity : glr.entmatrix, glr.viewmatrix);
    
    // ... rest of shadow face drawing
}
```

### Fix 4: Validate Dynamic Light Shadow Selection

**File**: [main.c](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/main.c)

Add debug logging to verify dynamic lights are being selected:

```c
// After line 1754 in GL_SelectShadowLights
Com_DPrintf("Shadow light %d: slot=%d dynamic=%d\n", i, glr.shadowmap_index[i], 
            dl->shadow == DL_SHADOW_DYNAMIC);
```

### Fix 5: Ensure BSP Models Use Correct Vertex Format in Shadow Pass

**File**: [world.c](file:///c:/Users/djdac/source/repos/WORR-2/src/rend_gl/world.c#L441)

Before `GL_DrawSolidFaces()` in `GL_DrawBspModel`, if shadow pass is active:

```c
if (glr.shadow_pass) {
    // Force matrix for BSP model entity
    GL_ForceMatrix(glr.entmatrix, glr.viewmatrix);
}

GL_DrawSolidFaces();
```

---

## Summary of Changes

| File | Function | Change |
|------|----------|--------|
| mesh.c | draw_alias_mesh | Use `GL_ForceMatrix` instead of `GL_LoadMatrix` |
| mesh.c | GL_DrawAliasModel | Set `gls.u_block_dirty = true` after setup_frame_scale |
| tess.c | GL_DrawFace | Force matrix before shadow face batching |
| world.c | GL_DrawBspModel | Force matrix before GL_DrawSolidFaces in shadow pass |

---

## Verification Checklist

- [ ] Static lights cast shadows from world geometry
- [ ] Static lights cast shadows from BSP model entities (func_rotating)
- [ ] Static lights cast shadows from MD2/MD5 item models
- [ ] Static lights cast shadows from MD5 player/monster models (correctly aligned)
- [ ] Dynamic lights (explosions) cast shadows from all entity types
- [ ] Shadow caching works correctly (static lights don't re-render unnecessarily)
- [ ] No performance regression from forced matrix uploads
