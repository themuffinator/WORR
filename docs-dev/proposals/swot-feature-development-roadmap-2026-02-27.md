# WORR SWOT and Task-Based Feature + Development Roadmaps

Date: 2026-02-27

## Purpose
Create a repository-grounded SWOT and convert it into actionable, task-based project roadmaps that can guide coordinated team execution.

## Baseline Snapshot (Repository-Derived)
- Codebase scale is substantial: approximately 733 `*.c`/`*.cpp`/`*.h`/`*.hpp` files and approximately 426k lines across `src/` and `inc/`.
- Workload concentration is heavily in gameplay and rendering:
  - `src/game/sgame`: 259 files, approximately 128k lines
  - `src/rend_gl`: 39 files, approximately 49k lines
  - `src/client`: 48 files, approximately 41k lines
  - `src/rend_rtx`: 65 files, approximately 38k lines
  - `src/rend_vk`: 15 files, approximately 26k lines
- cgame UI is already a large system:
  - `src/game/cgame/ui/worr.json`: 1869 lines
  - `src/game/cgame/ui/ui_widgets.cpp`: 114k+ bytes
- Build and release automation is mature:
  - Local staging standard: `.install/` via `tools/refresh_install.py`
  - Automated release paths: `.github/workflows/nightly.yml`, `.github/workflows/release.yml`
  - Platform packaging and metadata tooling under `tools/release/`
- Protocol compatibility has dedicated boundaries and tests:
  - `q2proto/` integrated as read-only in policy
  - `q2proto/tests/` contains protocol flavor build tests
- Existing technical debt is visible:
  - 200+ `TODO/FIXME/HACK/XXX` markers in first-party `src/` and `inc/` (excluding legacy and third-party trees)
  - Bots are structurally present but not functionally complete (`src/game/sgame/bots/bot_think.cpp` frame hooks are empty)
  - Local TODOs still include major items (`TODO.md`)
- Documentation volume is high and active:
  - `docs-dev/` carries extensive subsystem writeups (renderer, cgame, font, build, release)
  - Some docs show drift against current code paths, signaling curation needs

## SWOT

## Strengths
- Broad gameplay foundation already exists in `sgame`, with many game modes and systems wired (`src/game/sgame/g_local.hpp`, `src/game/sgame/gameplay/*`, `src/game/sgame/match/*`).
- Strong multi-renderer strategy is already implemented: OpenGL (`src/rend_gl`), native Vulkan raster (`src/rend_vk`), and RTX/vkpt (`src/rend_rtx`).
- Release and staging discipline is materially stronger than typical forks (`tools/refresh_install.py`, `tools/release/*`, nightly workflow automation).
- cgame and JSON UI architecture is robust and extensible (`src/game/cgame/*`, `src/game/cgame/ui/*`).
- Documentation culture is real and ongoing (`docs-dev/` has high-frequency change logs and design analyses).
- Clear policy boundaries already exist for critical risks:
  - no Vulkan-to-OpenGL fallback policy
  - q2proto compatibility guardrails

## Weaknesses
- Scope breadth is high enough to fragment focus without enforced project governance.
- Automated quality gates are underdeveloped for core engine/gameplay paths (CI is release-centric, not full PR validation-centric).
- Vulkan parity is still incomplete in multiple high-visibility areas (particle styles, beam styles, flare behavior, post-process parity).
- Bot behavior is not yet production-capable despite plumbing being present.
- Technical debt markers are spread across gameplay, client, renderer, and server paths.
- Cvar namespace modernization is only partially applied (`g_` still dominates many new sgame controls despite `sg_` preference).
- Dependency lifecycle complexity is high (multiple vendored versions of several libraries under `subprojects/`).
- Documentation freshness is uneven (some architecture docs lag current filenames/wiring).

## Opportunities
- Differentiate WORR through native Vulkan parity plus predictable performance improvements.
- Leverage already-rich game mode set into a clear competitive and cooperative offering roadmap.
- Exploit nightly + updater tooling to move toward rapid, measurable, low-friction iteration.
- Convert documentation volume into execution strength by binding work to project IDs and status workflows.
- Add targeted automated tests and smoke harnesses to reduce regression risk as refactors continue.
- Complete C++ migration with module boundaries that reduce long-term maintenance cost.
- Consolidate dependency versions and improve reproducibility/security posture.
- Use analytics/observability for performance baselines and release quality gates.

## Threats
- Regression risk is high due to surface area and currently limited automated gameplay/renderer test coverage.
- Cross-platform support can drift if changes are validated mainly on one host/toolchain.
- Feature creep can outrun finishing work unless work-in-progress limits are enforced.
- Upstream divergence from Q2REPRO, KEX, and reference idTech3 patterns can increase integration cost over time.
- Team coordination cost will rise with parallel renderer/gameplay/UI tracks unless roadmap ownership is explicit.
- Dependency sprawl increases update effort and potential security exposure windows.
- Documentation debt can lead to incorrect decisions when implementation and docs disagree.
- Public expectations around compatibility and re-release parity can be missed without milestone-level acceptance criteria.

## Project Backbone Model (Mandatory Operating Approach)

## Portfolio Structure
- Portfolio: `WORR 2026 Execution Portfolio`
- Projects:
  - `P-FEATURE`: player/admin-visible outcomes
  - `P-DEVELOPMENT`: engineering quality, architecture, and delivery capability

## Task Metadata Schema
Each task must include:
- `ID`: stable identifier (`FR-xx-Tyy` or `DV-xx-Tyy`)
- `Epic`: roadmap epic ID
- `Area`: subsystem (`rend_vk`, `cgame`, `sgame`, `tools/release`, etc.)
- `Priority`: `P0`, `P1`, `P2`
- `Dependencies`: IDs that must be completed first
- `Definition of Done`: explicit acceptance criteria

## Workflow States
- `Backlog`
- `Ready`
- `In Progress`
- `In Review`
- `Blocked`
- `Done`

## Cadence
- Weekly: backlog grooming and dependency resolution
- Biweekly: milestone review against exit criteria
- Per release train: roadmap delta review and reprioritization

## Definition of Ready
- Scope and subsystem boundaries are explicit.
- Dependencies are known and linked.
- Validation strategy is defined (build/test/runtime checks).

## Definition of Done
- Code merged and documented.
- Staging/packaging impact validated if applicable.
- Corresponding roadmap task marked complete.

## Feature Roadmap (Task-Based Project)

## Timeline
- Phase F1 (2026-03-01 to 2026-04-30): parity blockers and UI completion groundwork
- Phase F2 (2026-05-01 to 2026-08-31): major gameplay and renderer differentiation
- Phase F3 (2026-09-01 to 2026-12-31): feature hardening, polish, and release readiness

## Epic FR-01: Native Vulkan Gameplay Parity
Objective: close gameplay-visible parity gaps versus OpenGL while preserving native Vulkan policy.

Primary Areas: `src/rend_vk/*`, `src/client/renderer.cpp`, `docs-dev/vulkan-*.md`

Exit Criteria:
- Vulkan supports all essential gameplay rendering paths used in core multiplayer and campaign flows.
- Known parity blockers from Vulkan audits are closed or explicitly deferred with owner/date.

Tasks:
- [ ] `FR-01-T01` Implement Vulkan equivalents for particle style controls (`gl_partstyle` parity map to `vk_/r_` cvars).  
  Dependency: none. Priority: P0.
- [ ] `FR-01-T02` Implement Vulkan beam style parity (`gl_beamstyle` behavior equivalents).  
  Dependency: `FR-01-T01`. Priority: P0.
- [ ] `FR-01-T03` Add `RF_FLARE` behavior parity in Vulkan entity path.  
  Dependency: none. Priority: P1.
- [ ] `FR-01-T04` Complete MD2 and MD5 visual parity pass with map-driven validation scenes.  
  Dependency: none. Priority: P0.
- [ ] `FR-01-T05` Resolve remaining sky seam/artifact issues for all six faces and transitions.  
  Dependency: none. Priority: P0.
- [ ] `FR-01-T06` Finalize bmodel initial-state correctness on first render frame.  
  Dependency: `FR-01-T04`. Priority: P0.
- [ ] `FR-01-T07` Add Vulkan parity checklist doc and per-feature status table in `docs-dev/renderer/`.  
  Dependency: `FR-01-T01..T06`. Priority: P1.
- [ ] `FR-01-T08` Add Vulkan runtime debug overlays/counters for missing-feature detection.  
  Dependency: none. Priority: P1.

## Epic FR-02: Renderer Role Clarity (OpenGL vs Vulkan vs RTX)
Objective: ensure each renderer has a clearly defined role and quality target.

Primary Areas: `meson.build`, `src/client/renderer.cpp`, `src/rend_vk/*`, `src/rend_rtx/*`

Exit Criteria:
- Renderer selection behavior is explicit and documented.
- Vulkan raster and RTX path-tracing are clearly differentiated in functionality and messaging.

Tasks:
- [ ] `FR-02-T01` Produce renderer capability matrix (`opengl`, `vulkan`, `rtx`) and include cvar mapping.  
  Dependency: none. Priority: P0.
- [ ] `FR-02-T02` Add runtime command to dump active renderer capabilities to log/console.  
  Dependency: `FR-02-T01`. Priority: P1.
- [ ] `FR-02-T03` Align launch/debug presets with current renderer names and expected modes.  
  Dependency: none. Priority: P1.
- [ ] `FR-02-T04` Validate and document fallback/error behavior for missing renderer DLLs.  
  Dependency: none. Priority: P1.
- [ ] `FR-02-T05` Add parity smoke map sequence for each renderer in nightly validation.  
  Dependency: `DV-02-T03`. Priority: P1.
- [ ] `FR-02-T06` Publish renderer support policy page under `docs-user/` for end users.  
  Dependency: `FR-02-T01`. Priority: P2.

## Epic FR-03: JSON UI Rework Completion
Objective: complete modern menu coverage and remove remaining UX gaps for core settings and flows.

Primary Areas: `src/game/cgame/ui/*`, `src/game/cgame/ui/worr.json`, menu proposal docs

Exit Criteria:
- Main menu, in-game menu, and settings hierarchy are complete and stable.
- High-value missing widgets are implemented or replaced by approved alternatives.

Tasks:
- [ ] `FR-03-T01` Convert current menu proposal into implementation backlog with explicit widget tickets.  
  Dependency: none. Priority: P0.
- [ ] `FR-03-T02` Implement dropdown overlay behavior (no legacy spin-style fallback for new pages).  
  Dependency: `FR-03-T01`. Priority: P0.
- [ ] `FR-03-T03` Implement palette picker widget for color-centric settings.  
  Dependency: `FR-03-T01`. Priority: P1.
- [ ] `FR-03-T04` Implement crosshair tile/grid selector with live preview.  
  Dependency: `FR-03-T01`. Priority: P1.
- [ ] `FR-03-T05` Implement model preview widget for player visuals pages.  
  Dependency: `FR-03-T01`. Priority: P1.
- [ ] `FR-03-T06` Audit and complete settings page cvar wiring for Video/Audio/Input/HUD/Downloads.  
  Dependency: `FR-03-T02..T05`. Priority: P0.
- [ ] `FR-03-T07` Add menu regression checklist (navigation, conditionals, scaling, localization).  
  Dependency: `FR-03-T06`. Priority: P1.
- [ ] `FR-03-T08` Complete split between engine-side and cgame-side UI ownership where still mixed.  
  Dependency: `FR-03-T06`. Priority: P1.

## Epic FR-04: Bots and Match Experience
Objective: evolve bot and match systems from structural presence to reliable gameplay experience.

Primary Areas: `src/game/sgame/bots/*`, `src/game/sgame/match/*`, `src/game/sgame/gameplay/*`

Exit Criteria:
- Bots can join, navigate, fight, and participate in primary supported modes without obvious dead behavior.
- Match flow automation remains stable with bots in common scenarios.

Tasks:
- [ ] `FR-04-T01` Define bot MVP behavior set (spawn, roam, engage, objective awareness).  
  Dependency: none. Priority: P0.
- [ ] `FR-04-T02` Implement frame logic in `Bot_BeginFrame` and `Bot_EndFrame`.  
  Dependency: `FR-04-T01`. Priority: P0.
- [ ] `FR-04-T03` Add weapon selection heuristics and situational item use.  
  Dependency: `FR-04-T02`. Priority: P1.
- [ ] `FR-04-T04` Add team mode awareness (CTF/TDM/etc.) to bot utility state updates.  
  Dependency: `FR-04-T02`. Priority: P1.
- [ ] `FR-04-T05` Add map-level nav validation pass and bot spawn diagnostics.  
  Dependency: `FR-04-T02`. Priority: P1.
- [ ] `FR-04-T06` Add bot participation checks to match/tournament/map-vote flows.  
  Dependency: `FR-04-T02`. Priority: P1.
- [ ] `FR-04-T07` Provide bot tuning cvars in preferred naming convention (`sg_` for new controls).  
  Dependency: `FR-04-T01`. Priority: P2.

## Epic FR-05: Asset and Format Expansion
Objective: expand supported content formats without breaking current workflows.

Primary Areas: `src/renderer/*`, `src/rend_gl/*`, `src/rend_vk/*`, `inc/format/*`

Exit Criteria:
- Planned format support (IQM and extended BSP variants) has either landed or has approved implementation tracks with owners.

Tasks:
- [ ] `FR-05-T01` Build full format support matrix (current vs target) for MD2/MD3/MD5/IQM/BSP variants/DDS.  
  Dependency: none. Priority: P0.
- [ ] `FR-05-T02` Define IQM implementation plan and shared loader boundaries.  
  Dependency: `FR-05-T01`. Priority: P1.
- [ ] `FR-05-T03` Define extended BSP support plan (`IBSP29`, `BSP2`, `BSP2L`, `BSPX`) with compatibility rules.  
  Dependency: `FR-05-T01`. Priority: P1.
- [ ] `FR-05-T04` Add renderer-side format fallback diagnostics for unsupported assets.  
  Dependency: `FR-05-T01`. Priority: P2.
- [ ] `FR-05-T05` Add staged asset validation checks to packaging pipeline for new formats.  
  Dependency: `DV-02-T04`. Priority: P1.
- [ ] `FR-05-T06` Add user-facing docs describing supported asset formats and caveats.  
  Dependency: `FR-05-T01..T05`. Priority: P2.

## Epic FR-06: Audio, Feedback, and Accessibility
Objective: improve clarity and accessibility of gameplay feedback while preserving style.

Primary Areas: `src/client/sound/*`, `src/game/cgame/*`, localization and font docs

Exit Criteria:
- Critical feedback channels (audio cues, UI text, readability) are configurable and regression-tested.

Tasks:
- [ ] `FR-06-T01` Consolidate spatial audio follow-up backlog into implementation tasks.  
  Dependency: none. Priority: P1.
- [ ] `FR-06-T02` Complete graphical obituaries/chatbox enhancement track and integrate with localization.  
  Dependency: none. Priority: P1.
- [ ] `FR-06-T03` Add accessibility pass for text backgrounds, scaling, contrast defaults, and fallback fonts.  
  Dependency: none. Priority: P1.
- [ ] `FR-06-T04` Add presets for competitive readability vs immersive presentation.  
  Dependency: `FR-06-T03`. Priority: P2.
- [ ] `FR-06-T05` Add QA script/checklist for multi-language font rendering in main HUD/menu surfaces.  
  Dependency: `FR-06-T03`. Priority: P1.

## Epic FR-07: Multiplayer Operations and Match Tooling
Objective: harden map vote, match logging, tournament, and admin workflows.

Primary Areas: `src/game/sgame/match/*`, `src/game/sgame/menu/*`, `src/game/sgame/commands/*`

Exit Criteria:
- Admin and competitive flows are stable across map transitions and match-state changes.

Tasks:
- [ ] `FR-07-T01` Add end-to-end validation scenarios for map vote, mymap queue, and nextmap transitions.  
  Dependency: none. Priority: P1.
- [ ] `FR-07-T02` Harden tournament veto/replay flows with explicit error handling and state resets.  
  Dependency: none. Priority: P1.
- [ ] `FR-07-T03` Improve match logging artifact schema/versioning for downstream tooling.  
  Dependency: none. Priority: P2.
- [ ] `FR-07-T04` Add command-level audit for vote/admin privileges and abuse controls.  
  Dependency: none. Priority: P1.
- [ ] `FR-07-T05` Add server-operator docs for new competitive tooling and expected cvars.  
  Dependency: `FR-07-T01..T04`. Priority: P2.

## Epic FR-08: Online Ecosystem Foundations
Objective: prepare for web integration, identity, and service coupling without destabilizing core runtime.

Primary Areas: updater, release index tooling, future external services

Exit Criteria:
- Online roadmap is decomposed into incremental, testable tasks with security and reliability guardrails.

Tasks:
- [ ] `FR-08-T01` Define service boundary document for engine, game module, updater, and external web services.  
  Dependency: none. Priority: P1.
- [ ] `FR-08-T02` Define authentication and identity model (Discord OAuth or alternative) with threat model.  
  Dependency: `FR-08-T01`. Priority: P2.
- [ ] `FR-08-T03` Define server browser data contract between in-game UI and backend service.  
  Dependency: `FR-08-T01`. Priority: P2.
- [ ] `FR-08-T04` Define CDN/update channel strategy aligned with existing release index format.  
  Dependency: none. Priority: P2.
- [ ] `FR-08-T05` Stage a minimal public server deployment runbook and monitoring checklist.  
  Dependency: `FR-08-T01`. Priority: P2.

## Development Roadmap (Task-Based Project)

## Timeline
- Phase D1 (2026-03-01 to 2026-04-30): governance and quality-gate foundation
- Phase D2 (2026-05-01 to 2026-08-31): test automation, architecture cleanup, and CI scale-up
- Phase D3 (2026-09-01 to 2026-12-31): hardening, sustainability, and release excellence

## Epic DV-01: Project Governance and Team Workflow
Objective: make project tracking mandatory and consistent across all major work.

Primary Areas: `AGENTS.md`, `README.md`, `docs-dev/projects` process docs

Exit Criteria:
- All significant initiatives are tracked with epic/task IDs and lifecycle states.

Tasks:
- [ ] `DV-01-T01` Establish canonical project board template and required fields.  
  Dependency: none. Priority: P0.
- [ ] `DV-01-T02` Define naming conventions for epics/tasks/milestones and enforce in docs.  
  Dependency: `DV-01-T01`. Priority: P0.
- [ ] `DV-01-T03` Define WIP limits and escalation rules for blocked tasks.  
  Dependency: `DV-01-T01`. Priority: P1.
- [ ] `DV-01-T04` Add project status review ritual (weekly) with owners and outputs.  
  Dependency: `DV-01-T01`. Priority: P1.
- [ ] `DV-01-T05` Require roadmap task references in major PR descriptions and dev docs.  
  Dependency: `DV-01-T02`. Priority: P0.

## Epic DV-02: CI and Validation Pipeline Expansion
Objective: move from release-focused automation to continuous confidence for day-to-day development.

Primary Areas: `.github/workflows/*`, `tools/release/*`, build scripts

Exit Criteria:
- Every non-trivial change path has automated build/test/smoke coverage before merge.

Tasks:
- [ ] `DV-02-T01` Add PR CI workflow for configure + compile on Windows/Linux/macOS.  
  Dependency: none. Priority: P0.
- [ ] `DV-02-T02` Add matrix targets for external renderer libraries (`opengl`, `vulkan`, `rtx`) in CI.  
  Dependency: `DV-02-T01`. Priority: P0.
- [ ] `DV-02-T03` Add runtime smoke launch checks against `.install/` staging for each platform.  
  Dependency: `DV-02-T01`. Priority: P1.
- [ ] `DV-02-T04` Add staged payload validation for format/manifest completeness in PR CI.  
  Dependency: `DV-02-T01`. Priority: P1.
- [ ] `DV-02-T05` Add failure triage guide and flaky test quarantine workflow.  
  Dependency: `DV-02-T01`. Priority: P2.

## Epic DV-03: Automated Test Strategy
Objective: expand meaningful automated tests across protocol, gameplay, renderer, and tooling.

Primary Areas: `q2proto/tests`, `src/common/tests.c`, future test harness paths

Exit Criteria:
- Core regression-prone systems are covered by deterministic tests and smoke checks.

Tasks:
- [ ] `DV-03-T01` Integrate `q2proto/tests` into main CI path and publish result artifacts.  
  Dependency: `DV-02-T01`. Priority: P0.
- [ ] `DV-03-T02` Add unit-level tests for high-risk shared utilities (`files`, parsing, cvar helpers).  
  Dependency: none. Priority: P1.
- [ ] `DV-03-T03` Add deterministic server game rule tests for match-state transitions.  
  Dependency: none. Priority: P1.
- [ ] `DV-03-T04` Add renderer smoke scenes with pixel/hash tolerance checks for key features.  
  Dependency: `DV-02-T03`. Priority: P1.
- [ ] `DV-03-T05` Add bot scenario tests for spawn, navigation, and objective behavior.  
  Dependency: `FR-04-T02`. Priority: P2.
- [ ] `DV-03-T06` Add updater/release index parser tests for stable and nightly channels.  
  Dependency: none. Priority: P1.

## Epic DV-04: Architecture and Code Quality
Objective: reduce maintenance overhead and complete key modernization tracks.

Primary Areas: `meson.build`, `src/client/*`, `src/game/*`, naming policy docs

Exit Criteria:
- Module boundaries are cleaner, duplication is reduced, and coding standards are enforceable.

Tasks:
- [ ] `DV-04-T01` Define C/C++ migration target map with boundaries and no-go zones.  
  Dependency: none. Priority: P1.
- [ ] `DV-04-T02` Complete client/cgame ownership map for duplicated behavior paths.  
  Dependency: none. Priority: P1.
- [ ] `DV-04-T03` Add static analysis and warning-as-error policy for first-party code in CI.  
  Dependency: `DV-02-T01`. Priority: P1.
- [ ] `DV-04-T04` Create cvar namespace modernization plan (`g_` to `sg_` for new server-side controls).  
  Dependency: none. Priority: P1.
- [ ] `DV-04-T05` Track and burn down top 100 first-party TODO/FIXME markers by severity.  
  Dependency: none. Priority: P1.
- [ ] `DV-04-T06` Add subsystem ownership map (maintainers by directory) for faster review routing.  
  Dependency: none. Priority: P2.

## Epic DV-05: Performance and Observability
Objective: establish measurable performance baselines and regression visibility.

Primary Areas: renderers, server frame loop, profiling/logging tools

Exit Criteria:
- Baseline metrics exist and regressions can be identified quickly in development and CI.

Tasks:
- [ ] `DV-05-T01` Define canonical benchmark scenes/maps for renderer and gameplay performance checks.  
  Dependency: none. Priority: P1.
- [ ] `DV-05-T02` Add standardized perf capture commands and output schema.  
  Dependency: `DV-05-T01`. Priority: P1.
- [ ] `DV-05-T03` Add lightweight frame-time and subsystem timing instrumentation toggles.  
  Dependency: none. Priority: P1.
- [ ] `DV-05-T04` Add nightly trend report for key performance metrics.  
  Dependency: `DV-05-T02`. Priority: P2.
- [ ] `DV-05-T05` Add performance budget thresholds for major renderer and server paths.  
  Dependency: `DV-05-T01`. Priority: P2.

## Epic DV-06: Dependency Lifecycle and Security Hygiene
Objective: reduce dependency sprawl and improve update confidence.

Primary Areas: `subprojects/`, Meson wraps, release/build docs

Exit Criteria:
- Dependency versions are intentional, documented, and reviewable with lower drift risk.

Tasks:
- [ ] `DV-06-T01` Audit duplicate vendored versions and define active baseline per dependency.  
  Dependency: none. Priority: P0.
- [ ] `DV-06-T02` Remove or archive superseded dependency trees not needed for reproducible builds.  
  Dependency: `DV-06-T01`. Priority: P1.
- [ ] `DV-06-T03` Add dependency update checklist including security notes and regression tests.  
  Dependency: `DV-06-T01`. Priority: P1.
- [ ] `DV-06-T04` Add monthly dependency maintenance review cadence and owner.  
  Dependency: `DV-06-T01`. Priority: P2.

## Epic DV-07: Documentation Quality and Traceability
Objective: keep docs synchronized with implementation and projects.

Primary Areas: `docs-dev/`, `docs-user/`, root docs

Exit Criteria:
- Significant implementation changes have corresponding current docs with task references.

Tasks:
- [ ] `DV-07-T01` Add docs freshness audit for architecture docs that reference moved/renamed paths.  
  Dependency: none. Priority: P1.
- [ ] `DV-07-T02` Require task ID linkage in all new significant `docs-dev` change logs.  
  Dependency: `DV-01-T02`. Priority: P1.
- [ ] `DV-07-T03` Add concise subsystem index pages (`renderer`, `game`, `build`, `release`) for discoverability.  
  Dependency: none. Priority: P2.
- [ ] `DV-07-T04` Add user-doc parity pass whenever user-visible cvars/features are changed.  
  Dependency: none. Priority: P1.

## Epic DV-08: Release and Updater Hardening
Objective: ensure staged artifacts, update metadata, and updater behavior remain reliable under growth.

Primary Areas: `tools/release/*`, `tools/refresh_install.py`, `src/updater/worr_updater.c`

Exit Criteria:
- Release artifacts are consistently valid and updater behavior is deterministic across channels.

Tasks:
- [ ] `DV-08-T01` Add test fixtures for release index parsing edge cases (missing assets, mixed channels, malformed metadata).  
  Dependency: `DV-03-T06`. Priority: P1.
- [ ] `DV-08-T02` Add checksum/signature policy review for package trust model.  
  Dependency: none. Priority: P2.
- [ ] `DV-08-T03` Add rollback and failed-update recovery validation scenarios.  
  Dependency: none. Priority: P1.
- [ ] `DV-08-T04` Add release readiness checklist tied to roadmap milestone gates.  
  Dependency: `DV-01-T01`. Priority: P1.

## Immediate 90-Day Priority Queue (2026-03-01 to 2026-05-31)
- [ ] `P0` `FR-01-T01` Vulkan particle style parity
- [ ] `P0` `FR-01-T04` MD2/MD5 parity pass
- [ ] `P0` `FR-03-T02` JSON dropdown overlay
- [ ] `P0` `FR-04-T02` Bot frame logic implementation
- [ ] `P0` `DV-01-T01` Project board template rollout
- [ ] `P0` `DV-02-T01` PR CI workflow
- [ ] `P0` `DV-03-T01` Integrate q2proto tests into CI
- [ ] `P0` `DV-06-T01` Dependency baseline audit

## Governance Note
This roadmap is intended to be the live planning source for WORR 2026 execution. Any significant new initiative should be added here first as an epic/task set (or linked as a child project) before implementation starts.
