# DOF Depth Attachments on Demand

## Summary
To improve post‑process stability on recent NVIDIA drivers, the renderer now
requests a depth texture attachment for DOF only when DOF is actually active
(non‑zero `dof_strength`). When DOF is inactive, the scene FBO uses the standard
depth‑stencil renderbuffer, which avoids depth‑texture related driver issues in
normal gameplay.

## Behavior
- If `r_dof` is enabled but `dof_strength` is zero, the FBO is created without
  a depth texture (DOF is not active anyway).
- When `dof_strength` becomes non‑zero (inventory/wheel), the FBO is re‑initialized
  with a depth texture and DOF is enabled for that frame.

## Rationale
This avoids attaching depth textures to the scene FBO during typical play while
keeping DOF available when actually in use. It also reduces exposure to driver
regressions related to depth texture attachments.
