# sgame qu3e gib+barrel physics integration (2026-02-17)

## Goal
Introduce a high-quality, efficient external rigid-body library into WORR sgame and apply it directly to:
- `gib` entities
- `misc_explobox` barrels

without breaking existing idTech2 world-collision behavior.

## Library Selection
Selected library: `qu3e` (Randy Gaul)

Why this library was chosen for this implementation pass:
- Purpose-built for fast game rigid-body simulation.
- Very small footprint (few source files, no external runtime deps).
- Efficient enough for high transient-count entities like gibs.
- Easy to embed directly into `sgame` and keep deterministic frame-step ownership server-side.

## License Compatibility
Project license: GPLv2 (`LICENSE` at repository root).

Embedded library license: zlib (`src/game/sgame/third_party/qu3e/LICENSE.txt`).

Compatibility note:
- zlib is GPLv2-compatible for redistribution in this combined work.
- The original qu3e license text is preserved in-tree.

## Source Integration
qu3e was vendored to:
- `src/game/sgame/third_party/qu3e/`

Included qu3e compilation units were wired into `sgame` in `meson.build`.

New sgame-side integration module:
- `src/game/sgame/gameplay/g_qu3e_physics.hpp`
- `src/game/sgame/gameplay/g_qu3e_physics.cpp`

## Runtime Architecture
A dedicated bridge (`SG_QU3EPhysics_*`) was added to run one qu3e solver step per server frame.

Design model:
1. Discover/track eligible entities by classname each frame.
2. Maintain qu3e dynamic bodies for `gib`/`misc_explobox` and kinematic proxy bodies for live players/monsters.
3. Sync entity origin/linear/angular velocity into qu3e.
4. Run qu3e scene step.
5. Blend solved linear/angular velocities back into WORR entities.
6. Let existing idTech movement (`G_RunEntity`, `G_Physics_*`) continue handling world traces, map collision, triggers, and gameplay touch logic.
7. Route `barrel_touch` through `SG_QU3EPhysics_HandleBarrelTouch()` when enabled, so barrel pushes are injected as qu3e impulses instead of legacy-only `M_walkmove`.

This keeps map/world collision native while improving rigid-body interaction response between tracked dynamic props.

## Applied Entities
Tracked entity classes:
- `gib`
- `misc_explobox`

Practical effects:
- Gibs now participate in external rigid-body velocity resolution against tracked bodies.
- Barrels now receive qu3e impulses from touch interactions and collide against qu3e kinematic pusher proxies (players/monsters), making push response observable beyond barrel-vs-gib cases.

## New sgame cvars
Added in `g_main.cpp` and exported in `g_local.hpp`:
- `sg_phys_qu3e_enable` (master toggle)
- `sg_phys_qu3e_gibs` (enable/disable gib tracking)
- `sg_phys_qu3e_barrels` (enable/disable barrel tracking)
- `sg_phys_qu3e_velocity_blend` (solver output blend factor into entity velocity)

## Key Integration Hooks
- Init: `SG_QU3EPhysics_Init()` in `InitGame()`
- Per-frame update: `SG_QU3EPhysics_RunFrame()` in `G_RunFrame_()` before main entity simulation loop
- Shutdown: `SG_QU3EPhysics_Shutdown()` in `ShutdownGame()`

## Notes on behavior and constraints
- Gravity inside qu3e scene is intentionally set to zero to avoid double-gravity with idTech physics.
- idTech remains source of truth for world/static collision and trigger interactions.
- qu3e currently acts as an external rigid-body velocity solver for tracked entities.

## Build Verification
Compilation validated with:
- `meson compile -C builddir`

Result:
- Build succeeded.
- Third-party qu3e emits warnings in its own code (non-fatal); no build break.

## Follow-up candidates
- Add a dedicated per-target compile warning suppression block for third-party qu3e files (optional hygiene).
- Extend tracking to additional debris classes if desired.
- Add optional rotational transform sync (axis-angle) if visual orientation coupling is later desired.
