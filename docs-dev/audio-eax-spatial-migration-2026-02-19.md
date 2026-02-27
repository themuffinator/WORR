# Q2RTXPerimental EAX Spatial Audio Migration (2026-02-19)

## Summary
This change retires WORR's prior occlusion/material-probe acoustics runtime and replaces the active OpenAL spatial environment path with the Q2RTXPerimental EAX environment model.

The replacement introduces:
- EAX environment profile support through `S_SetEAXEnvironmentProperties`.
- OpenAL-side EAX/EFX property application with EAX-first and standard-reverb fallback.
- `client_env_sound`/`env_sound` map zone selection via LOS + radius checks.
- Runtime interpolation between EAX environments.
- Imported Q2RTXPerimental EAX profile assets under `assets/eax/*.json`.

## Engine Changes

### 1. Sound API surface
- Added `sfx_eax_properties_t` in `inc/client/sound/sound.h`.
- Added EAX effect ID constants (`SOUND_EAX_EFFECT_*`) in `inc/client/sound/sound.h`.
- Added `S_SetEAXEnvironmentProperties(...)` declaration and implementation.
- Extended private sound backend vtable (`sndapi_t`) with:
  - `set_eax_effect_properties`

### 2. OpenAL backend migration
- Implemented backend EAX property setter in `src/client/sound/al.cpp`:
  - `AL_SetEAXEffectProperties(...)`
- Added Q2RTXPerimental-style EAX zone controller in `src/client/sound/al.cpp`:
  - EAX profile loading (`eax/*.json`)
  - env zone parsing from BSP entity string (`client_env_sound` and `env_sound`)
  - LOS/radius effect selection
  - environment interpolation
- OpenAL channel spatialization now uses an EAX auxiliary send path directly.
- `al_eax` (default `1`) and `al_eax_lerp_time` (default `1.0`) were added.

### 3. Legacy acoustics deactivation
- Core occlusion calculation in `src/client/sound/main.cpp` now resolves to no-op output (0 occlusion).
- `s_occlusion` default set to `0`.
- DMA backend EAX setter is stubbed (`false`) and remains non-EAX.

## Assets
Imported Q2RTXPerimental EAX profiles:
- `assets/eax/abandoned.json`
- `assets/eax/alley.json`
- `assets/eax/arena.json`
- `assets/eax/auditorium.json`
- `assets/eax/bathroom.json`
- `assets/eax/carpetedhallway.json`
- `assets/eax/cave.json`
- `assets/eax/chapel.json`
- `assets/eax/city.json`
- `assets/eax/citystreets.json`
- `assets/eax/concerthall.json`
- `assets/eax/dizzy.json`
- `assets/eax/drugged.json`
- `assets/eax/dustyroom.json`
- `assets/eax/forest.json`
- `assets/eax/hallway.json`
- `assets/eax/hangar.json`
- `assets/eax/library.json`
- `assets/eax/livingroom.json`
- `assets/eax/mountains.json`
- `assets/eax/museum.json`
- `assets/eax/paddedcell.json`
- `assets/eax/parkinglot.json`
- `assets/eax/plain.json`
- `assets/eax/psychotic.json`
- `assets/eax/quarry.json`
- `assets/eax/room.json`
- `assets/eax/sewerpipe.json`
- `assets/eax/smallwaterroom.json`
- `assets/eax/stonecorridor.json`
- `assets/eax/stoneroom.json`
- `assets/eax/subway.json`
- `assets/eax/underpass.json`

## Full Credits

### Upstream project
- Project: **Q2RTXPerimental**
- Repository: `https://github.com/PolyhedronStudio/Q2RTXPerimental`
- Upstream reference used for this migration: `f22ebec1ad49d67aaeda786957cbf90f4e93390e`

### EAX system source inspiration and transplanted design
- Q2RTXPerimental client EAX workflow:
  - `src/baseq2/clgame/clg_eax.cpp`
  - `src/baseq2/clgame/clg_eax_effects.h`
  - `src/baseq2/clgame/local_entities/clg_local_env_sound.cpp`
- Q2RTXPerimental OpenAL EAX application path:
  - `src/client/sound/al.c`
  - `src/client/sound/main.c`
- Q2RTXPerimental EAX profile data:
  - `baseq2rtxp/eax/*.json`

### Original and upstream authorship chain
- id Software: original Quake II sound architecture foundation.
- Andrey Nazarov and q2pro lineage contributors: OpenAL backend lineage used in both projects.
- NVIDIA Q2RTX lineage contributors: EFX/EAX-era renderer/audio ecosystem foundations used by Q2RTXPerimental.
- PolyhedronStudio Q2RTXPerimental contributors: map-zone EAX environment system and profile pack adopted here.

### License notice
- The adopted logic and data remain subject to their original upstream licensing terms.
- WORR keeps attribution in-source and in this change document for provenance clarity.

## Files Changed
- `inc/client/sound/sound.h`
- `src/client/sound/sound.h`
- `src/client/sound/main.cpp`
- `src/client/sound/al.cpp`
- `src/client/sound/dma.cpp`
- `assets/eax/*.json`
- `docs-dev/audio-eax-spatial-migration-2026-02-19.md`
