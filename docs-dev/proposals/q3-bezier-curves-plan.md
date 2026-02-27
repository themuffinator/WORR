# Quake 3-Style Bezier Curves Implementation Plan

## Executive Summary

This plan outlines the work needed to add Quake 3-style quadratic bezier patch surfaces to WORR while preserving full compatibility with legacy Quake II servers, clients, and demos. The implementation spans map format decisions, renderer tessellation, collision model updates, tooling, and QA. The approach explicitly avoids changes to `q2proto/` and keeps legacy Q2 `.bsp` maps and demos working unchanged.

## Goals

- Render Q3-style quadratic bezier patches with parity to idTech3 behavior (visual shape, shading, and LOD).
- Support correct collision and physics on curved surfaces for both client prediction and server-side movement.
- Maintain full compatibility with legacy Quake II maps, servers, demos, and protocol.
- Keep the system modular so renderer backends (GL/Vulkan) and collision can evolve independently.

## Non-Goals

- No changes to network protocol or demo format.
- No forced migration of existing Q2 maps or assets.
- No requirement for Q3 shader language parity (patches should work with existing Q2 material system).

## Constraints and Compatibility

- Legacy Q2 `.bsp` must load without modification and behave identically.
- New bezier features must not alter server/client netcode behavior.
- `q2proto/` remains read-only.
- Use idTech3 codebases (Quake3e, baseq3a) as reference for patch data structures, tessellation, and collision logic.

## Reference Implementations to Study

- Quake3e and baseq3a patch geometry (tessellation and collision).
- Files to mirror conceptually:
  - `tr_curve.c` / `tr_surface.c` for tessellation and draw surfaces.
  - `cm_patch.c` for collision patch subdivision.
  - `qfiles.h` patch surface definitions and BSP lump layout.

## Feature Definition (Q3 Bezier Patches)

Q3 patches are quadratic bezier surfaces defined by a grid of control points (typically 3x3 per patch). They are tessellated into triangle grids based on subdivision settings, with normals and texture/lightmap coordinates interpolated per-vertex.

Key behaviors to match:

- Subdivision based on curvature and configurable granularity (Q3 `r_subdivisions` semantics).
- Consistent edge tessellation to prevent cracks between adjacent patches.
- Smooth normals across patch surface (tangent space optional but desirable for modern lighting).

## Complexity Assessment

| Area | Complexity | Notes |
| --- | --- | --- |
| Map format / asset pipeline | High | Must extend or add a new map format without breaking legacy Q2. |
| Renderer tessellation | High | Requires correct LOD, normals, and batching integration. |
| Collision / physics | High | Must match Q3 behavior while keeping Q2 physics stable. |
| Tooling / map compiler | High | Patch data must be authored and compiled reliably. |
| QA / compatibility | Medium-High | Must validate both old and new content on multiple backends. |
| Performance tuning | Medium | Patch density can be expensive; need scalable settings. |

## Architecture Options and Decision Tasks

### Option A: Q2 BSP Extension Lumps (Recommended)

- Extend the existing Q2 BSP format with new lumps containing patch control grids.
- Legacy engines ignore unknown lumps; WORR loads them when present.
- Requires custom map compiler or post-process tool.

### Option B: Dual Map Format Support (Q2 BSP + Q3 BSP)

- Detect Q3 BSP version and use a separate loader path.
- Highest compatibility with existing Q3 tooling but more engine complexity.
- Risk: divergent paths for collision/rendering.

### Option C: Pre-Tessellate at Compile Time

- Convert patches to triangle meshes in the BSP, store only triangles.
- Simpler runtime, but loses true curve detail and LOD flexibility.
- Heavier BSP sizes and potential visual artifacts.

**Decision Task:** Select the approach that best balances compatibility and maintainability. Option A is preferred to keep single loader path and minimize forked code, but it requires compiler/tool support.

## Work Plan

### Phase 0: Research and Specification (1-2 weeks)

- Study Q3 patch behavior in Quake3e and baseq3a.
- Define how patch data maps into WORR structures.
- Decide on map format strategy (Option A/B/C).
- Define cvars and defaults (e.g., `r_subdivisions`, `r_patchLodBias`).
- Draft a minimal test map or conversion pipeline for patches.

### Phase 1: Data Format and Loader (2-4 weeks)

- Add new data structures for patch surfaces and control grids.
- Implement BSP loader extensions:
  - Detect new lumps or map version.
  - Load patch control points, texture/lightmap coords.
  - Validate and sanitize input (control grid dimensions, bounds).
- Update map model caching and memory management.
- Add debug visualization hooks for patch grids.

### Phase 2: Renderer Tessellation and LOD (3-6 weeks)

- Implement quadratic bezier surface tessellation:
  - Evaluate 3x3 control points into a grid.
  - Subdivide based on curvature and `r_subdivisions`.
  - Build triangle indices with consistent winding.
- Compute vertex normals:
  - Use partial derivatives or averaged triangle normals.
  - Ensure continuity across patch seams.
- Integrate patch meshes into world draw surface pipeline:
  - Create patch draw surfaces at map load.
  - Merge or batch with existing static geometry where possible.
- Lightmap and texture coordinate handling:
  - Interpolate texture coords and lightmap coords from control points.
  - Handle lightmap stitching or padding to avoid seams.
- Backend support:
  - GL: VBO/IBO pipeline for patches.
  - Vulkan/VKPT: equivalent mesh buffers and draw calls.
- Add debug cvars for patch LOD and wireframe.

### Phase 3: Collision and Physics (3-6 weeks)

- Implement patch collision models using tessellated grids:
  - Mirror Q3 `CM_SubdividePatch` behavior.
  - Generate collision triangles at map load with configurable density.
- Integrate with collision queries:
  - Ensure trace and box collision respect patch triangles.
  - Confirm player movement on curved surfaces remains stable.
- Add collision LOD policy:
  - Separate collision subdivision settings from rendering (e.g., `cm_patchSubdiv`).
- Validate against slope limits, step handling, and entity movement.

### Phase 4: Tooling and Map Pipeline (3-8 weeks)

- Choose or build patch-capable map compiler path:
  - Option A: Extend existing Q2 map compiler or write a post-process tool.
  - Option B: Accept Q3 BSP and add conversion/import logic.
- Ensure authoring workflow:
  - Provide guidelines for Radiant-based patch creation.
  - Define how materials/textures map to Q2 assets.
- Add asset validation:
  - Check for patch control grid correctness.
  - Ensure lightmap UV ranges are valid.
- Documentation:
  - Author a mapping guide for bezier patches in WORR.

### Phase 5: QA, Compatibility, and Performance (2-4 weeks)

- Compatibility testing:
  - Load a corpus of legacy Q2 maps with no regressions.
  - Join legacy servers and play demos that load Q2 maps.
- New content testing:
  - Rendered geometry correctness vs. reference Q3 engine.
  - Collision behavior on curved ramps and bowls.
- Performance profiling:
  - Stress test with high patch density.
  - Measure CPU tessellation cost at load and runtime.
- Regression tests:
  - Verify no changes in network messages or demo playback.

## Task Breakdown (Detailed)

### Data Format

- Define a patch surface structure in engine memory.
- Decide on BSP lump layout or versioning.
- Implement loader validation and error reporting.
- Add version flags in map metadata to track patch presence.

### Tessellation

- Implement bezier evaluation:
  - Quadratic basis function for u/v.
  - Grid generation with even subdivisions.
- Prevent cracks:
  - Snap edge tessellation counts across adjacent patches.
  - Maintain a shared edge topology key.
- Build normals:
  - Compute derivatives in u and v directions.
  - Normalize and optionally smooth by adjacency.
- Integrate with lightmap generation and texture mapping.

### Rendering Integration

- Add patch surfaces to draw list.
- Expand batching to handle patch materials.
- Ensure debug visualization of control points and grid.
- Add user-facing cvars and defaults.

### Collision Integration

- Build collision triangle mesh from tessellation.
- Integrate with existing trace and sweep logic.
- Confirm BSP leaf/cluster queries include patch triangles.

### Tooling

- Extend or build map compiler pipeline.
- Provide sample maps for QA.
- Add optional command-line tools for patch validation.

### QA and Compatibility

- A/B visual comparison with Q3 for sample patches.
- Demo playback on legacy Q2 maps.
- Join legacy Q2 servers to verify no protocol impact.

## Key Challenges and Risks

- **Map format compatibility:** Extending Q2 BSP without breaking tools is high risk. Mitigate with versioned lumps and strict validation.
- **Cracks and seams:** Patch adjacency must maintain identical edge tessellation to avoid visible gaps.
- **Lightmap seams:** Lightmap interpolation on curved surfaces can create visible discontinuities.
- **Performance:** High patch density can inflate vertex counts and collision complexity; need robust LOD controls.
- **Collision stability:** Smooth surfaces can trigger small positional jitter in legacy physics; needs tuning.
- **Tooling availability:** Existing Q2 compilers may not support patches; may require new tools or a converter.
- **Renderer parity:** Ensure GL and Vulkan backends both handle patch meshes consistently.

## Definition of Done

- Legacy Q2 maps load and render exactly as before.
- A patch test map renders correctly with smooth curves and stable lighting.
- Collision on patch surfaces matches reference Q3 behavior within tolerance.
- No changes to network protocol or demo format.
- Documentation exists for authoring, compiling, and tuning patches.

## Open Questions

- Which map format extension is most sustainable for WORR (Option A/B/C)?
- How to integrate patch shading with current Q2 material pipeline?
- What default subdivision values balance visual quality and performance?
- Should patch tessellation be CPU-only or allow GPU tessellation later?

## Suggested Initial Milestones

1. Decision on map format strategy and prototype loader.
2. Basic render-only patch tessellation in a test map.
3. Collision patch mesh integration.
4. Tooling pipeline finalized with a sample map pack.
5. Full QA on legacy content and regression checks.
