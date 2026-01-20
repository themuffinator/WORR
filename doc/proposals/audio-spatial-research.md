# Audio System Deep Research: Spatial World Audio Improvements

## Executive Summary

The WORR-2 audio system already includes sophisticated spatial audio features implemented via OpenAL with EFX extensions. This document analyzes the current implementation and identifies realistic, high-impact improvements for spatial world audio.

---

## Current Implementation Analysis

### Occlusion System
**Files**: `main.cpp`, `al.cpp`, `dma.cpp`, `sound.h`

| Feature | Implementation |
|---------|----------------|
| Ray sampling | 16 rays (1 direct + 8 around source + 8 around listener) |
| Material weights | Glass (0.25), Grate (0.3), Soft (0.6), Wood (0.75), Metal (0.85) |
| Temporal smoothing | Attack rate 25.0, Release rate 8.0 |
| Update interval | 50ms per channel with entity-based jitter |
| Diffraction approximation | Peripheral ray blending (40% weight) |

**Strengths**: Multi-ray sampling with material awareness provides realistic muffling.

**Limitations**: 
- No true portal/room-based propagation
- Rays use direct line-of-sight; sound cannot "bend" around corners via pathfinding
- No per-material transmission loss curves (frequency-dependent attenuation)

---

### Reverb System
**Files**: `al.cpp`

| Feature | Implementation |
|---------|----------------|
| EFX reverb presets | 26 presets (generic through psychotic) |
| Environment selection | Floor material-based via step_id mapping |
| Transitions | Smooth lerping over `al_reverb_lerp_time` (default 3s) |
| Room estimation | Dimension probing via raycasts |

**Strengths**: Smooth reverb transitions prevent jarring changes.

**Limitations**:
- Reverb is global, not per-source; all sounds share the same reverb
- No early reflections based on actual geometry
- Room estimation is listener-centric only

---

### Doppler Effect
**Files**: `al.cpp`

| Feature | Implementation |
|---------|----------------|
| Velocity smoothing | Exponential smoothing (rate 12) |
| Speed of sound | 13500 units/sec (configurable) |
| Velocity clamping | 30-4000 units/sec |
| Entity detection | RF_DOPPLER flag + projectile effect fallback |

**Strengths**: Velocity smoothing prevents pitch spikes from net jitter.

**Limitations**: None significant—this is well-implemented.

---

### Distance Attenuation
**Files**: `al.cpp`, `main.cpp`

| Feature | Implementation |
|---------|----------------|
| Distance model | `AL_LINEAR_DISTANCE_CLAMPED` |
| Attenuation formula | `(1 - dist) * scale` with SOUND_FULLVOLUME threshold |

**Limitations**:
- No air absorption (high-frequency rolloff over distance)
- Linear model may be less realistic than inverse-distance for large spaces

---

### HRTF (Binaural Audio)
**Files**: `qal.cpp`

| Feature | Implementation |
|---------|----------------|
| Cvar | `al_hrtf` (0=off, 1=default, >1=force enable) |
| Extension | `ALC_SOFT_HRTF` |
| Per-source spatialization | `AL_SOFT_source_spatialize` supported |

**Strengths**: Modern HRTF support for headphone users.

**Limitations**: No UI exposure or user guidance for enabling HRTF.

---

## Identified Improvements

### 1. **Air Absorption (High-Frequency Distance Rolloff)** ⭐ High Impact, Low Effort
Sound in air naturally loses high frequencies over distance. This is different from occlusion—it affects all distant sounds, not just occluded ones.

**Implementation**:
```cpp
// In AL_Spatialize or during source update
float distance = VectorDistance(listener_origin, source_origin);
float air_absorption = 1.0f - Q_clipf(distance / S_AIR_ABSORPTION_DISTANCE, 0.0f, 1.0f);
// Apply to per-source filter or as AL_AIR_ABSORPTION_FACTOR if using EFX
```

**OpenAL Support**: `AL_AIR_ABSORPTION_FACTOR` in EFX already exists.

**Effort**: ~1 hour

---

### 2. **Distance Model Improvement** ⭐ Medium Impact, Low Effort
Replace `AL_LINEAR_DISTANCE_CLAMPED` with `AL_INVERSE_DISTANCE_CLAMPED` for more natural falloff in large spaces. This provides a more gradual attenuation that matches real-world acoustics.

**Implementation**:
```cpp
qalDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
```

**Considerations**: May require tuning reference distance and rolloff factor per sound type.

**Effort**: ~30 minutes + testing

---

### 3. **Portal/Aperture-Based Occlusion** ⭐⭐ High Impact, Medium Effort
When occlusion traces detect doorways or window openings, calculate an alternative sound path through the aperture with appropriate diffraction.

**Approach**:
1. When direct path is occluded, probe for nearby portals (doorways, windows)
2. If a portal path exists: sound origin = portal center, gain *= portal_size / ideal_size
3. Apply slight low-pass to simulate diffraction bending

**Data Structure**:
```cpp
typedef struct {
    vec3_t center;
    vec3_t mins, maxs;
    float area;
} audio_portal_t;
```

**Considerations**: 
- Can use existing brush entities or derive from area portals
- Should cache portal visibility per listener position

**Effort**: 4-8 hours

---

### 4. **Early Reflections** ⭐⭐ High Impact, High Effort
Add a single early reflection per sound source based on nearest reflective surface. This dramatically improves spatial perception without the cost of full raytraced reflections.

**Approach**:
1. Cast rays in cardinal directions from sound origin
2. First hit within threshold distance becomes reflection point
3. Create virtual source at mirrored position with attenuated gain
4. Use per-source auxiliary send to EFX reverb aux slot

**OpenAL Support**: EFX auxiliary sends (`AL_AUXILIARY_SEND_FILTER`) support per-source reflection routing.

**Effort**: 8-16 hours

---

### 5. **Per-Source Reverb Sends** ⭐ Medium Impact, Medium Effort
Currently reverb is global. Per-source auxiliary sends would allow:
- Distant sounds to have more reverb
- Occluded sounds to route more through "room tail"
- Close sounds to be dry

**Implementation**:
```cpp
// During AL_Spatialize
float reverb_send = Q_clipf(dist / S_MAX_REVERB_DISTANCE, 0.2f, 1.0f);
if (occlusion_mix > 0.5f)
    reverb_send *= 1.5f;  // Occluded = more reverb
qalSource3i(ch->srcnum, AL_AUXILIARY_SEND_FILTER, s_reverb_slot, 0, reverb_filter);
qalSourcef(ch->srcnum, AL_ROOM_ROLLOFF_FACTOR, reverb_send);
```

**Effort**: 4-6 hours

---

### 6. **Directional Sources (Sound Cones)** ⭐ Medium Impact, Low Effort
Some sounds should have directional emission (e.g., NPCs speaking, directional alarms). OpenAL supports this natively.

**Implementation**:
```cpp
// For directional sources
qalSourcef(srcnum, AL_CONE_INNER_ANGLE, 60.0f);
qalSourcef(srcnum, AL_CONE_OUTER_ANGLE, 180.0f);
qalSourcef(srcnum, AL_CONE_OUTER_GAIN, 0.3f);
qalSource3f(srcnum, AL_DIRECTION, dir[0], dir[1], dir[2]);
```

**Considerations**: Requires entity direction data and per-sound cone definitions.

**Effort**: 2-4 hours

---

### 7. **Enhanced Material Transmission** ⭐ Medium Impact, Low Effort
Current material weights are gain-only. Add frequency-dependent filtering per material.

| Material | Gain | High-Freq Cutoff |
|----------|------|------------------|
| Glass | 0.25 | 4000 Hz |
| Grate | 0.30 | No change |
| Wood | 0.75 | 2000 Hz |
| Concrete | 0.15 | 800 Hz |
| Metal | 0.85 | 1500 Hz |

**Implementation**:
```cpp
typedef struct {
    const char *name;
    float gain;
    float cutoff_hz;  // NEW: frequency cutoff
} material_occlusion_t;
```

**Effort**: 2-3 hours

---

### 8. **Ambient Sound Zones** ⭐⭐ High Impact, High Effort
Define spatial zones with ambient soundscapes that blend as the player moves.

**Approach**:
- Map editor defines zone brushes with ambient properties
- Runtime blends between zones based on listener position
- Supports area-specific ambient loops (machinery, wind, water)

**Considerations**: Requires map data and editor support.

**Effort**: 16+ hours

---

## Priority Recommendations

| Priority | Improvement | Impact | Effort |
|----------|-------------|--------|--------|
| 1 | Air Absorption | High | Low |
| 2 | Distance Model Switch | Medium | Low |
| 3 | Enhanced Material Transmission | Medium | Low |
| 4 | Per-Source Reverb Sends | Medium | Medium |
| 5 | Portal-Based Occlusion | High | Medium |
| 6 | Directional Sources | Medium | Low |
| 7 | Early Reflections | High | High |
| 8 | Ambient Sound Zones | High | High |

---

## Technical Notes

### OpenAL Extensions Used
- `ALC_EXT_EFX` - Effects and filters ✅
- `ALC_SOFT_HRTF` - Binaural processing ✅
- `AL_SOFT_source_spatialize` - Per-source HRTF ✅
- `AL_SOFT_loop_points` - Seamless looping ✅
- `AL_EXT_float32` - High-quality samples ✅

### Not Currently Used (Available)
- `AL_AIR_ABSORPTION_FACTOR` - Air absorption per source
- `AL_CONE_*` - Directional sound emission
- `AL_AUXILIARY_SEND_*` - Per-source effect routing

---

## Conclusion

The WORR-2 audio system has excellent foundations with sophisticated occlusion, reverb, and Doppler systems. The highest-impact improvements with lowest effort are:

1. **Air absorption** - adds realism with minimal code
2. **Distance model tuning** - better falloff curves
3. **Material-frequency coupling** - richer occlusion effects

Medium-term goals should focus on:
- Portal-based sound propagation for "around the corner" effects
- Per-source reverb sends for spatial depth

Long-term goals:
- Early reflections based on geometry
- Ambient zone system for environment-specific soundscapes
