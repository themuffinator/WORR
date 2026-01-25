# Hall-of-Mirrors (HOM) Effects After Shadow Mapping Refactors  
## Causes, Diagnostics, and Post-Phase Snags (Q2REPRO / idTech2)

This document explains **why hall-of-mirrors (HOM) artifacts can appear after implementing Phases 1 and 2** of the shadow mapping refactor, and what additional pitfalls to watch for after **Phases 3 and 4** (filtering, PCSS, CSM).

It is written specifically for:
- idTech2 / Quake II render architecture
- Q2REPRO-style GLSL/ARB pipelines
- Multi-pass rendering with FBO-based shadow maps

---

## 1. What a HOM Effect Actually Means in idTech2

In Quake/Quake II renderers, a hall-of-mirrors effect almost always means:

> **Some pixels in the main framebuffer were not written this frame.**

The GPU shows:
- Old backbuffer contents
- Previous frame data
- Undefined memory patterns

HOM is **not a texture bug** and **not a shadow math bug**.  
It is almost always:
- GL state leakage, or
- World/surface visibility corruption

---

## 2. Most Common Causes After Phases 1–2 (In Order of Likelihood)

---

## 2.1 Viewport or Scissor State Not Restored (Most Common)

### Symptom Pattern
- Shadow “light snapshots” render correctly
- Everything outside those areas shows HOM
- Often only a corner of the screen updates

### Typical Cause
Shadow pass sets:
- `glViewport(0,0,shadowRes,shadowRes)`
- `glScissor(...)`

…but the main pass never restores full-screen viewport/scissor.

### Required Fix
Before *every* main view render:

- Bind default framebuffer
- Set viewport to full window size
- Disable scissor test (or reset it)

**Debug test:**  
Force `glDisable(GL_SCISSOR_TEST)` and reset viewport just before drawing the world.  
If HOM disappears → this was the cause.

---

## 2.2 Framebuffer / Draw Buffer State Leakage

### Typical Shadow-Pass Changes
- `glBindFramebuffer(shadowFBO)`
- `glDrawBuffer(GL_NONE)` (depth-only)
- `glColorMask(false,false,false,false)`

If any of these persist into the main pass, **no color writes occur**.

### Required Fix
At the start of the main pass:

- `glBindFramebuffer(GL_FRAMEBUFFER, 0)`
- `glDrawBuffer(GL_BACK)`
- `glColorMask(true,true,true,true)`
- `glDepthMask(true)`

**Strong recommendation:**  
Add a debug-only assert function that checks these states before world rendering.

---

## 2.3 Depth / Culling State Leakage

These don’t always blank the whole screen, but they often cause **large missing regions**.

### Common Mistakes
- `glDepthFunc()` changed (e.g. `GL_GREATER`, `GL_EQUAL`)
- `glDepthRange()` modified
- `glDisable(GL_DEPTH_TEST)` left active
- `glEnable(GL_POLYGON_OFFSET_FILL)` not disabled
- `glCullFace()` flipped for shadow rendering and not restored

### Debug Test
Temporarily force before world draw:
- Disable culling
- `glDepthFunc(GL_LEQUAL)`
- Disable polygon offset

If HOM changes dramatically, depth/cull state leakage is involved.

---

## 2.4 Corrupting Quake II Visibility or Surface Chains

### Why This Happens After Phase 1
Phase 1 introduces:
- New BSP traversal
- New surface marking
- Light-volume-based occluder selection

If you reuse or overwrite:
- `surf->visframe`
- `r_framecount`
- `visframecount`
- `texturechain` pointers

…the **main view can think the world is invisible**.

### Result
- Holes in world rendering
- Pixels never written → HOM

### Required Rule
Shadow passes must:
- Use **separate per-shadow markers** (e.g. `shadowframe`)
- Never touch main-view globals
- Never rebuild or invalidate main texture chains

**Debug test:**  
Clear the screen every frame.  
If HOM disappears but geometry is missing → surface visibility corruption.

---

## 2.5 Early Returns or Partial Frame Paths (Debug Modes)

Common when adding:
- Light snapshot views
- Debug overlays
- Shadow-only modes

If code:
- Draws snapshots
- Returns early
- Skips full-screen clear or world draw

→ HOM outside snapshot regions.

### Fix
In snapshot-only modes:
- Clear framebuffer explicitly
- Or draw a full-screen background quad

---

## 3. Why Shadow Texture Bugs Rarely Cause HOM Directly

Incorrect:
- Shadow layer indices
- Cube face selection
- Sampling math

Usually only corrupt **shadowed lighting**, not framebuffer writes.

HOM only appears when:
- Rendering stops writing pixels
- Or visibility logic skips geometry

---

## 4. Post-Phase 3 Snags (Filtering, PCF, VSM, EVSM, PCSS)

---

### 4.1 VSM / EVSM Artifacts (Not HOM, But Easy to Misdiagnose)

#### Common Issues
- Light bleeding
- Dark halos
- Black splotches

#### Causes
- Variance too small or unclamped
- EVSM exponent overflow
- Using NEAREST filtering with VSM

#### Mitigations
- Clamp variance
- Add bleed reduction
- Use linear filtering and/or mipmaps
- Clamp EVSM exponent ranges

---

### 4.2 Point Light Seams with Large Filters

When using cube-style point lights:
- PCF/PCSS kernels can sample outside face bounds
- Causes seams or popping near cube edges

#### Mitigations
- Clamp UVs
- Reduce kernel size near edges
- Prefer cubemap shadow samplers if available

---

### 4.3 Bias Becomes Filter-Dependent

Bias that works for:
- Hard shadows  
may fail for:
- Wide PCF
- PCSS
- VSM blur

#### Mitigations
- Scale bias with filter radius
- Use normal-offset bias in receiver space
- Expose per-method tuning cvars

---

### 4.4 PCSS Performance Collapse

PCSS is expensive:
- Blocker search
- Variable-radius filtering
- Multiplied by number of lights

#### Mitigations
- Limit PCSS to top-N lights
- PCSS only for sun or primary light
- Dynamic sample reduction based on screen size

---

## 5. Post-Phase 4 Snags (Cascaded Shadow Maps)

---

### 5.1 Shimmering During Camera Movement

Caused by:
- Cascade projections moving every frame

#### Mitigations
- Snap cascade bounds to texel-sized increments in light space
- Stabilize cascade frustums

---

### 5.2 Cascade Seams and Popping

Hard transitions between cascades cause visible lines.

#### Mitigations
- Overlap cascades
- Blend based on depth
- Keep bias/filtering consistent across cascades

---

### 5.3 Over-Aggressive Culling Per Cascade

Too-tight caster or receiver culling causes missing shadows.

#### Mitigations
- Conservative bounds
- Add padding to caster volumes
- Debug cascade overlays

---

## 6. Fast HOM Triage Checklist

Use in this order:

1. **Force clear every frame**
2. **Reset viewport + disable scissor**
3. **Reset FBO, draw buffer, color mask**
4. **Force sane depth/cull state**
5. **Disable new shadow surface marking temporarily**

Each step isolates a class of bugs in minutes.

---

## 7. Structural Recommendation (Prevents Most HOM Bugs)

Treat shadow rendering as a *foreign subsystem*:

- `ShadowPass_Begin()` → save critical GL state
- `ShadowPass_End()` → restore it
- `MainPass_Begin()` → force known-good baseline

Do **not** rely on incremental state restoration.

---

## 8. Key Takeaway

HOM after shadow refactors is almost never about shadows themselves.

It means:
> **Your main view stopped writing pixels somewhere.**

Fix state discipline and visibility integrity first.  
Shadow math comes later.

---
