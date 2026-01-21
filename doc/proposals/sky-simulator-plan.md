# Modern Sky Simulator + Sky Portals (OpenGL 4.1) Plan

## Summary
This document proposes a modern, customizable, efficient sky simulator for the WORR OpenGL renderer, including particle/volumetric clouds, dynamic weather, and optional sky portals (3D skybox via separate BSP). The system targets PC, OpenGL 4.1, and integrates with existing map assets and new sky scripts referenced by worldspawn `sky` keys.

## Goals
- **High-quality yet efficient** sky rendering suitable for PC, with scalable quality settings.
- **Modern physically inspired rendering** (atmospheric scattering, sun/moon/sky light) without compute shaders.
- **Asset-driven** and **scripted** skies with per-map customization.
- **Weather systems** that can be enabled per map with script parameters.
- **Sky portal** support for 3D sky boxes rendered from a separate BSP.
- **Minimal disruption** to existing maps and skyboxes; clear differentiation between legacy skybox usage and scripted simulation.

## Non-Goals
- Full-blown meteorology simulation or real-world accurate forecasts.
- Mandatory replication for multiplayer (only settings are propagated).
- Engine-wide lighting rework beyond sky/ambient integration.

## External Library Options (GPL-Compatible)
*Decision point: choose a library only after a short evaluation and license audit.*

Potential options to evaluate:
- **Atmospheric scattering reference implementations** under MIT/BSD-style licenses (GPL-compatible). Examples include Bruneton-style precomputed atmosphere shaders (commonly BSD/Apache/MIT). Verify exact license compatibility with this project’s license.
- **Noise generation** libraries (FastNoise Lite - MIT) for cloud density/coverage and weather variation.
- **Sky/atmosphere shaders** from GPL-compatible sources with OpenGL 4.x support.

Evaluation criteria:
- License compatibility with WORR/Q2REPRO.
- OpenGL 4.1 compatibility and no compute shader dependency.
- Ability to integrate with existing renderer architecture.

## System Overview
The system is split into layers and subsystems, each with clear inputs/outputs and quality switches.

### 1) Sky Script + Asset Pipeline
- **Sky scripts** define sky behavior, referencing assets (textures, LUTs), lighting, and weather presets.
- **Worldspawn `sky` key** determines behavior:
  - **Legacy skybox** (existing behavior): `sky` references a classic skybox name.
  - **Simulator**: `sky` references a `.sky` script (e.g., `sky my_sky_sim`).
  - **Portal**: `sky` references a portal configuration (details below).
- Scripts are loaded by the client renderer at map load, and can be cached for hot reload in development mode.

### 2) Atmosphere + Sun/Moon + Stars
- **Atmospheric scattering** via precomputed LUTs (Rayleigh/Mie) using OpenGL 4.1 fragment shaders.
- **Sun/moon disk** with configurable angular radius, color, and intensity.
- **Stars** via static cubemap or procedural starfield.
- **Time-of-day** can be scripted or static.

### 3) Cloud Systems
Two layers that can be combined:
- **Particle clouds**
  - Billboarded sprites or instanced quads.
  - GPU instancing with per-particle attributes (density, phase, altitude).
  - Uses 3D noise textures or 2D noise slices for softness.
- **Volumetric clouds (lightweight)**
  - Ray-marched in the fragment shader using 3D noise textures.
  - Adjustable step counts and fallback to 2D textures for low quality.

### 4) Weather Systems
- **Precipitation**: rain/snow rendered as GPU instanced particles with depth-based fading.
- **Fog and haze** adjustments tied to atmospheric settings.
- **Lightning**: timed light pulses + optional sky flash texture.
- **Wind**: used to animate cloud layers and precipitation drift.

### 5) Sky Portals (3D Sky Boxes)
- **Separate BSP** provides a small “sky scene.”
- Renderer loads a **portal BSP** when the map’s `sky` key specifies a portal.
- The portal BSP is rendered from a designated origin, producing a cube or 2D sky texture.
- Portal is composited as the far background; main map renders normally.

## Detailed Implementation Plan
*This workload is large; it is broken into phases with checkpoints.*

### Phase 0: Design + Data Definition (1–2 weeks)
- Define sky script format:
  - Atmospheric parameters, sun/moon, stars, cloud layers, weather presets, quality tiers, and references to textures/LUTs.
- Define worldspawn `sky` key parsing:
  - `sky` -> legacy name (existing behavior)
  - `sky sim:<script>` -> simulated sky
  - `sky portal:<portal_name>` -> portal BSP
- Define data structures in the renderer for sky settings, LUT handles, and weather state.
- Decide on external libraries (if any) after evaluation and license audit.

### Phase 1: Atmosphere Core (2–3 weeks)
- Implement a **sky dome** render pass:
  - Precompute LUTs (Rayleigh/Mie) at load time or on-demand.
  - Render sky gradients with sun disk and optional moon disk.
- Integrate **ambient light**/sky light contributions to existing lighting systems (basic only).
- Add **stars** via skybox or procedural.
- Add script parsing + parameter validation.

### Phase 2: Cloud Systems (3–4 weeks)
- Implement **particle cloud layer** with GPU instancing:
  - Asset-driven textures and noise.
  - Density/coverage via script.
- Implement **volumetric cloud layer** with ray-march:
  - OpenGL 4.1 fragment shader approach.
  - Adjustable quality: step count, resolution scaling.
- Add global cloud shadowing (optional, low frequency).

### Phase 3: Weather Systems (2–3 weeks)
- Add **rain/snow** GPU particle system.
- Add **fog/haze** integration tied to weather scripts.
- Add **lightning** flash + optional cloud illumination.
- Add **wind** integration for cloud drift and precipitation.

### Phase 4: Sky Portals (2–3 weeks)
- Add **portal BSP loader** (client renderer).
- Render portal BSP to **cubemap or 2D texture** each frame or at a lower rate.
- Apply the portal texture as the sky background.
- Handle **origin/scale** mapping between main BSP and portal BSP.
- Add debugging visualizations for portal origin and coverage.

### Phase 5: Optimization + Quality (2–4 weeks)
- Add **quality levels** and performance cvars.
- Implement **temporal accumulation** for volumetric clouds to reduce cost.
- Use **LUT caching** and shared shader permutations.
- Profile GPU/CPU usage and adjust default settings.

### Phase 6: Tooling + Documentation (1–2 weeks)
- Document the sky script format and sample scripts.
- Provide guidelines for artists: skybox + clouds + weather assets.
- Document portal BSP creation and usage.

## Sky Script Design (Proposed)
*Final schema depends on engine config system.*

```ini
sky {
	name = "stormy_sunset"
	atmosphere = {
		lut = "textures/sky/atmo_lut"
		sun_color = "1.0 0.85 0.7"
		sun_intensity = 8.0
		sun_dir = "0.3 0.7 0.6"
		moon_enable = true
	}
	clouds = {
		layer0 = {
			type = "particle"
			texture = "textures/sky/clouds01"
			density = 0.6
		}
		layer1 = {
			type = "volumetric"
			noise = "textures/sky/noise3d"
			coverage = 0.4
		}
	}
	weather = {
		rain = true
		rain_rate = 0.3
		wind_dir = "1 0 0"
		wind_speed = 3.0
		lightning = true
	}
}
```

## Sky Portal Design
- **Worldspawn `sky` key** uses a portal prefix:
  - `sky portal:sky_portal_bsp` loads `maps/sky_portal_bsp.bsp`.
- The portal BSP includes a **portal origin** entity:
  - e.g., `info_skyportal` entity with `origin` and `angles`.
- Rendering options:
  - **Render-to-cubemap** for high quality.
  - **Render-to-2D** for lower cost.
  - Update frequency adjustable via cvar (every frame, every N frames).

## Rendering Pipeline (OpenGL 4.1)
1. **Portal pass** (optional): render portal BSP to cubemap/2D texture.
2. **Sky pass**: render atmosphere + sun/moon + stars + clouds.
3. **Main world pass**.
4. **Weather pass**: rain/snow + lightning flashes.

## Performance and Quality Controls
- Quality tiers: Low / Medium / High
- Options:
  - Cloud resolution scaling
  - Volumetric step count
  - Portal update frequency
  - Particle counts
  - Sky LUT resolution

## Testing & Validation
- **Performance capture** on low and mid-range GPUs.
- Verify sky script parsing and fallback behavior.
- Ensure portal sky does not break PVS/vis or cause z-fighting artifacts.
- Validate that legacy skyboxes remain unchanged.

## Open Questions / Decisions Needed
- Which external library (if any) best fits atmosphere or noise generation.
- Preferred format for sky script (existing config format vs new custom syntax).
- Whether portal BSP should be a hard dependency or optional fallback to skybox.

