# WORR Spatial Audio Capabilities

> **Immersive 3D Audio System with Real-Time Occlusion, Environmental Reverb, and HRTF Support**

---

## Overview

WORR features a comprehensive spatial audio system built on OpenAL with EFX (Effects Extension) support. The system delivers immersive, physically-modeled audio through advanced occlusion, environmental reverb, Doppler effects, and Head-Related Transfer Function (HRTF) processing for headphone users.

---

## Key Features at a Glance

| Feature | Description |
|---------|-------------|
| **Multi-Ray Occlusion** | 16-ray system with material-aware filtering |
| **EFX Reverb** | 26 environment presets with smooth transitions |
| **Doppler Effect** | Per-projectile tracking with velocity smoothing |
| **HRTF (Binaural)** | Accurate 3D positioning for headphones |
| **Air Absorption** | Distance-based high-frequency rolloff |
| **Per-Source Reverb Sends** | Individual reverb routing per sound |
| **Material Transmission** | Frequency-dependent filtering through surfaces |

---

## Occlusion System

The occlusion system simulates how sounds are muffled and attenuated when obstructed by world geometry. Unlike simple binary on/off systems, WORR uses a multi-ray probability model with material-aware weighting.

### Multi-Ray Sampling

The system traces **16 rays** per sound source to determine occlusion:

- **1 direct ray**: Listener → source center
- **8 peripheral rays around source**: Cardinal + diagonal offsets
- **8 peripheral rays around listener**: Catch sounds "peeking" around corners

This multi-sample approach produces smooth, realistic transitions as objects partially obstruct sounds.

### Diffraction Approximation

Peripheral rays are blended with the direct trace using a configurable weight (default 40%), approximating how sound "bends" around corners through diffraction. This prevents the jarring cutoffs typical of single-ray occlusion.

### Material-Based Transmission

Occlusion traces identify surface materials and apply appropriate attenuation and frequency response:

| Material | Gain Weight | HF Cutoff | Effect |
|----------|-------------|-----------|--------|
| **Glass / Window** | 0.25 | 4000 Hz | Significant muffling, preserves upper-mids |
| **Grate / Mesh / Vent** | 0.30 | Clear | Light attenuation, no HF loss |
| **Soft (Cloth / Carpet)** | 0.60 | 2000 Hz | Moderate absorption |
| **Wood / Plywood** | 0.75 | 2000 Hz | Light absorption |
| **Metal / Steel** | 0.85 | 1500 Hz | Minimal absorption, muffled highs |
| **Concrete / Cement** | 0.15 | 800 Hz | Heavy muffling, deep bass only |

### Temporal Smoothing

To prevent "flutter" from geometry edges or minimal obstructions:

- **Attack rate**: 25.0 (quick occlusion onset)
- **Release rate**: 8.0 (gradual return to clarity)
- **Dead-zone threshold**: 0.1 (ignores tiny occlusion fractions)
- **Update interval**: 50ms per channel with entity-based jitter

### OpenAL Implementation

- Per-source `AL_FILTER_LOWPASS` filters control high-frequency rolloff
- `AL_GAIN` reduction proportional to occlusion factor
- `AL_DIRECT_FILTER` applies both gain and HF attenuation atomically

### CVars

```
s_occlusion          1       Enable/disable occlusion system
s_occlusion_strength 1.0     Scale final occlusion intensity (0.0-2.0)
```

---

## Environmental Reverb

The reverb system uses OpenAL EFX to create convincing room acoustics with 26 environmental presets and smooth transitions between zones.

### Preset Library

| # | Preset | Use Case |
|---|--------|----------|
| 0 | Generic | Default fallback |
| 1 | Padded Cell | Anechoic/damped spaces |
| 2 | Room | Small indoor spaces |
| 3 | Bathroom | Tiled reflective spaces |
| 4 | Living Room | Furnished medium rooms |
| 5 | Stone Room | Hard-surfaced chambers |
| 6 | Auditorium | Large performance spaces |
| 7 | Concert Hall | Grand acoustic venues |
| 8 | Cave | Natural rock formations |
| 9 | Arena | Sports/combat arenas |
| 10 | Hangar | Massive industrial spaces |
| 11 | Carpeted Hallway | Damped corridors |
| 12 | Hallway | Open corridors |
| 13 | Stone Corridor | Hard-surfaced passages |
| 14 | Alley | Outdoor narrow spaces |
| 15 | Forest | Open natural spaces |
| 16 | City | Urban environments |
| 17 | Mountains | Wide open terrain |
| 18 | Quarry | Open industrial pits |
| 19 | Plain | Flat open terrain |
| 20 | Parking Lot | Outdoor concrete spaces |
| 21 | Sewer Pipe | Cylindrical tunnels |
| 22 | Underwater | Submerged acoustics |
| 23 | Drugged | Psychoacoustic effect |
| 24 | Dizzy | Disorienting effect |
| 25 | Psychotic | Extreme effect |

### Environment Selection

Reverb presets are selected based on:

1. **Floor material detection**: The player's footstep surface identifies zone type
2. **Room dimension probing**: Raycasts estimate space volume and shape
3. **JSON configuration**: Maps can define custom environment zones

### Smooth Transitions

When moving between reverb zones:

- **Lerp time**: 3 seconds default (`al_reverb_lerp_time`)
- All EFX parameters interpolate smoothly
- Prevents jarring reverb "pops" when crossing zone boundaries

### Dimension Estimation

The system probes room dimensions using a 16-direction raycast pattern:

```cpp
// Cardinal and diagonal probes at 45° intervals
// + Vertical probes for ceiling/floor
```

Results inform the reverb's:
- Decay time (larger spaces = longer tails)
- Diffusion (complex spaces = more scatter)
- Early reflections (nearby surfaces = shorter delays)

### CVars

```
al_reverb            1       Enable EFX reverb system
al_reverb_lerp_time  3.0     Transition time between presets (seconds)
```

---

## Doppler Effect

The Doppler system applies realistic pitch shifting to moving sound sources, particularly effective for fast-moving projectiles.

### Implementation

- **Per-entity velocity tracking**: Smoothed, clamped velocities derived from interpolated positions
- **Speed of sound**: 13,500 Quake units/second (configurable)
- **Velocity smoothing**: Exponential smoothing (rate 12) prevents pitch spikes from network jitter

### Flagged Projectiles

The following projectile types receive Doppler processing:

| Category | Projectiles |
|----------|-------------|
| **Energy Weapons** | Blaster, Hyperblaster, Blue Blaster, Ion Ripper |
| **Rockets** | Rocket Launcher, Heat Seeker, Phalanx |
| **Plasma** | Plasma Gun bolts |
| **Advanced** | Disruptor/Tracker bolts, BFG ball, Disintegrator |
| **Monsters** | Vore homing pods |
| **Quake 1** | Plasmaball, Tesla bolt |

### Fallback Detection

When `RF_DOPPLER` is unavailable, Doppler enables for entities with these effects:

- `EF_ROCKET` / `EF_BLASTER` / `EF_HYPERBLASTER`
- `EF_BLUEHYPERBLASTER` / `EF_PLASMA`
- `EF_IONRIPPER` / `EF_BFG` / `EF_TRACKER`

### Loop Merge Bypass

Doppler sources bypass the loop-merging optimization to preserve individual entity velocities.

### CVars

```
al_doppler           1       Doppler factor (0 = disabled)
al_doppler_speed     13500   Speed of sound (units/sec)
al_doppler_min_speed 30      Minimum source speed for Doppler
al_doppler_max_speed 4000    Maximum source speed (clamped)
al_doppler_smooth    12      Velocity smoothing rate
```

---

## HRTF (Head-Related Transfer Function)

HRTF provides accurate 3D audio positioning for headphone users by simulating how sound reaches each ear differently based on direction.

### Features

- **Extension**: `ALC_SOFT_HRTF` (OpenAL Soft)
- **Per-source spatialization**: `AL_SOFT_source_spatialize` supported
- **Quality**: Uses OpenAL Soft's built-in HRTF database

### Benefits

- Accurate elevation perception (above/below)
- Front/back distinction
- Precise left/right positioning
- Natural "inside the head" localization for stereo headphones

### CVars

```
al_hrtf              0       HRTF mode: 0=off, 1=default, >1=force enable
```

---

## Air Absorption

High frequencies naturally attenuate over distance in air. This effect adds realism to distant sounds without requiring occlusion.

### Implementation

Two approaches based on OpenAL capabilities:

**EFX path** (preferred):
- Uses `AL_AIR_ABSORPTION_FACTOR` per source
- Factor scales linearly with distance

**Fallback path**:
- Folds absorption into the direct low-pass filter
- No additional effect slot required

### Behavior

- Maximum absorption at configurable distance (default 2048 units)
- **Underwater exemption**: Automatically disabled when submerged (avoids double-filtering)

### CVars

```
al_air_absorption          1       Enable air absorption
al_air_absorption_distance 2048    Full absorption distance (units)
```

---

## Per-Source Reverb Sends

Unlike global-only reverb systems, WORR routes each sound source to the reverb effect individually, allowing:

- **Distance-based reverb**: Distant sounds have more reverb
- **Occlusion-boosted reverb**: Muffled sounds route more through room tail
- **Close sounds stay dry**: Nearby sources remain clear

### Implementation

```cpp
// Reverb send scales with:
// - Distance from listener
// - Occlusion factor (occluded = more reverb)
// - Minimum send level (default 0.2)
// - Occlusion boost multiplier (default 1.5)
```

Uses `AL_AUXILIARY_SEND_FILTER` for per-source effect routing.

### CVars

```
al_reverb_send                    1     Enable per-source reverb sends
al_reverb_send_distance        2048     Full reverb send distance
al_reverb_send_min             0.2      Minimum reverb send level
al_reverb_send_occlusion_boost 1.5      Occluded sound reverb multiplier
```

---

## Distance Attenuation

### Distance Models

WORR supports two OpenAL distance models:

| Model | Behavior | Best For |
|-------|----------|----------|
| **Linear** | Constant falloff rate | Consistent attenuation |
| **Inverse** | Gradual natural rolloff | Large open spaces |

The inverse model (`AL_INVERSE_DISTANCE_CLAMPED`) better matches real-world acoustics for outdoor and large interior spaces.

### CVars

```
al_distance_model    1       0=linear, 1=inverse
```

---

## Underwater Audio

When the player is submerged (`RDF_UNDERWATER`), the audio system applies:

- Global low-pass filtering (configurable HF gain)
- Modified reverb (switches to `UNDERWATER` preset if available)
- Disabled air absorption (water has different propagation)

### CVars

```
s_underwater         1       Enable underwater audio effects
s_underwater_gain_hf 0.25    High-frequency gain when submerged
```

---

## Technical Architecture

### OpenAL Extensions Used

| Extension | Purpose | Status |
|-----------|---------|--------|
| `ALC_EXT_EFX` | Effects and filters | ✅ Active |
| `ALC_SOFT_HRTF` | Binaural processing | ✅ Active |
| `AL_SOFT_source_spatialize` | Per-source HRTF | ✅ Active |
| `AL_SOFT_loop_points` | Seamless looping | ✅ Active |
| `AL_EXT_float32` | High-quality samples | ✅ Active |
| `AL_AIR_ABSORPTION_FACTOR` | Air absorption | ✅ Active |

### Per-Channel State

Each sound channel maintains:

```cpp
struct channel_t {
    float occlusion;           // Smoothed occlusion factor
    float occlusion_target;    // Raw traced occlusion
    float occlusion_cutoff;    // Current HF cutoff
    float occlusion_cutoff_target; // Target HF cutoff
    int   occlusion_time;      // Last update timestamp
    bool  no_merge;            // Bypass loop merge for Doppler
    // ... additional state
};
```

### Source Files

| File | Purpose |
|------|---------|
| `src/client/sound/sound.h` | Constants and interface definitions |
| `src/client/sound/main.cpp` | Core sound management and occlusion queries |
| `src/client/sound/al.cpp` | OpenAL backend with EFX implementation |
| `src/client/sound/dma.cpp` | Software mixer fallback with biquad filters |
| `src/client/sound/qal.cpp` | OpenAL function loading and HRTF setup |

---

## Configuration Summary

### Enable/Disable Features

```
s_occlusion          1       Occlusion system
al_reverb            1       EFX reverb
al_doppler           1       Doppler effect
al_hrtf              0       HRTF (headphones)
al_air_absorption    1       Air absorption
al_reverb_send       1       Per-source reverb
s_underwater         1       Underwater effects
```

### Quality Tuning

```
s_occlusion_strength        1.0     Occlusion intensity
al_reverb_lerp_time         3.0     Reverb transition time
al_doppler_smooth          12       Doppler smoothing rate
al_distance_model           1       Distance attenuation model
al_air_absorption_distance  2048    Air absorption range
```

---

## Performance Considerations

### Optimization Features

- **Rate-limited occlusion**: 50ms update interval per channel
- **Entity-based jitter**: Prevents all sources updating simultaneously
- **Loop merging**: Combines identical looping sounds (except Doppler sources)
- **Cached reverb probes**: Room dimensions update only on movement

### Recommended Settings

| Hardware | Occlusion | Reverb | HRTF |
|----------|-----------|--------|------|
| Low-end | `s_occlusion 0` | `al_reverb 1` | `al_hrtf 0` |
| Mid-range | `s_occlusion 1` | `al_reverb 1` | `al_hrtf 0` |
| High-end | `s_occlusion 1` | `al_reverb 1` | `al_hrtf 1` |
| Headphones | `s_occlusion 1` | `al_reverb 1` | `al_hrtf 1` |

---

## Future Roadmap

The following enhancements are under consideration:

### Near-Term
- **Directional sources (sound cones)**: Speaker-facing sounds for alarms, NPCs
- **Portal-based propagation**: Sound paths through doorways/windows

### Long-Term
- **Early reflections**: Geometry-based first reflection points
- **Ambient sound zones**: Location-specific ambient soundscapes
- **Advanced diffraction**: True corner-bending via pathfinding

---

## Conclusion

WORR's spatial audio system combines proven techniques with modern OpenAL extensions to deliver immersive, believable sound. The multi-ray occlusion, smooth reverb transitions, and HRTF support create a soundscape that enhances gameplay awareness while maintaining atmosphere.

For optimal experience:
- **Speakers**: Use default settings with reverb enabled
- **Headphones**: Enable HRTF (`al_hrtf 1`) for accurate 3D positioning
- **Competitive**: Consider `s_occlusion 1` for directional awareness
