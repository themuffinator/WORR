# Doppler Effect Implementation Analysis

## Executive Summary

The WORR Doppler implementation is solid with proper velocity smoothing, speed clamping, and loop merge bypass for Doppler sources. However, several issues and improvements have been identified that would enhance the effect's realism and robustness.

---

## Current Implementation Overview

### Key Components

| Component | Location | Purpose |
|-----------|----------|---------|
| `doppler_state_t` | `al.cpp:70-74` | Per-entity velocity tracking state |
| `AL_GetEntityVelocity` | `al.cpp:1096-1166` | Computes smoothed source velocity |
| `AL_UpdateListenerVelocity` | `al.cpp:1771-1801` | Tracks player velocity |
| `AL_EntityHasDoppler` | `al.cpp:1080-1094` | Detects Doppler-enabled entities |
| `al_doppler_changed` | `al.cpp:627-635` | CVar callback for factor/speed |

### CVars

| CVar | Default | Purpose |
|------|---------|---------|
| `al_doppler` | 1 | Doppler factor (0 disables) |
| `al_doppler_speed` | 13500 | Speed of sound (units/sec) |
| `al_doppler_min_speed` | 30 | Minimum source speed for effect |
| `al_doppler_max_speed` | 4000 | Maximum source speed (clamped) |
| `al_doppler_smooth` | 12 | Exponential smoothing rate |

---

## Identified Issues

### 1. **Velocity Computation Has No Direction Validation** ⚠️ Medium

**Location**: `AL_GetEntityVelocity` (lines 1136-1138)

**Problem**: Velocity is computed as `(origin - prev_origin) / dt`. If an entity teleports (e.g., goes through a portal, respawns), the computed velocity will be astronomically high for one frame, even with clamping.

**Current Mitigation**: The 250ms `dt` limit (line 1128) catches teleports by resetting state when `dt_ms > 250`, but this threshold is too long - projectiles moving at 4000 units/sec cover 1000 units in 250ms.

**Fix**:
```cpp
// After computing instant velocity, check for discontinuities
float travel_dist = VectorLength(instant) * dt;
float expected_max = max_speed * dt * 1.5f; // Allow 50% overshoot
if (travel_dist > expected_max) {
    // Likely teleport - reset state instead of using this velocity
    VectorCopy(origin, state->origin);
    VectorClear(state->velocity);
    state->time = now;
    return true; // Return zero velocity
}
```

**Effort**: Low (15 minutes)

---

### 2. **Listener Velocity Uses Wrong Frame Time** ⚠️ Low

**Location**: `AL_UpdateListenerVelocity` (line 1790)

**Problem**: Uses `cls.frametime` which may be zero or stale after `S_Update` is called. The doc notes mention this was fixed by moving `cls.frametime = 0.0f` to after `S_Update`, but the code still uses `cls.frametime` directly without a fallback.

**Current Code**:
```cpp
float lerp = 1.0f - expf(-smooth * Q_clipf(cls.frametime, 0.0f, 0.1f));
```

**Fix**: If `cls.frametime <= 0`, use a sensible default or skip smoothing entirely:
```cpp
float dt = cls.frametime > 0.0f ? Q_clipf(cls.frametime, 0.0f, 0.1f) : 0.016f;
float lerp = 1.0f - expf(-smooth * dt);
```

**Effort**: Trivial (5 minutes)

---

### 3. **Entity Velocity Tracks Position, Not Actual Velocity** ⚠️ Medium

**Location**: `AL_GetEntityVelocity` (entire function)

**Problem**: For entities that have a `velocity` field in their state (e.g., players, some NPCs), we compute velocity from position deltas instead of using the actual velocity. For projectiles this is fine (position delta matches velocity), but for entities with complex motion it may not.

**Improvement**: Check if the entity state has a usable velocity and prefer that:
```cpp
// For players, use predicted velocity if available
if (entnum >= 1 && entnum <= cl.csr.max_clients) {
    // Player entities may have velocity in playerstate
    // This would require additional state tracking
}
```

**Effort**: Medium (requires evaluating which entity types reliably have velocity)

---

### 4. **No Doppler for One-Shot Sounds** 🔧 Improvement

**Location**: `AL_UpdateSourceVelocity` (lines 1168-1179)

**Problem**: Doppler only applies to looping sounds (entity sounds). One-shot sounds like explosions, weapon fire from moving enemies, etc. don't get Doppler processing.

**Improvement**: Track fixed_origin one-shot sounds with Doppler:
```cpp
// In AL_PlayChannel, for fixed_origin sounds from moving entities:
if (ch->fixed_origin && ch->entnum > 0) {
    // Could sample entity velocity at play time and set AL_VELOCITY
    vec3_t vel;
    if (AL_GetEntityVelocity(ch->entnum, vel)) {
        qalSource3f(ch->srcnum, AL_VELOCITY, AL_UnpackVector(vel));
    }
}
```

**Considerations**: One-shot sounds are short, so the Doppler effect is minimal. This may not be worth the complexity.

**Effort**: Medium (2-4 hours)

---

### 5. **Missing `al_doppler_speed` Change Callback Chain** 🐛 Bug

**Location**: `AL_Init` (lines 913-914)

**Problem**: `al_doppler_min_speed`, `al_doppler_max_speed`, and `al_doppler_smooth` don't have `changed` callbacks. If a user changes these values at runtime, the change won't take effect until the next frame's velocity computation uses the new values via `Cvar_ClampValue`.

**Current Impact**: Low - these cvars work correctly because they're read fresh each frame.

**Potential Issue**: If we add any cached state based on these cvars, it won't update.

**No fix needed** - current implementation is correct, just noting for future maintainability.

---

### 6. **Doppler State Array Size Mismatch** ⚠️ Potential Bug

**Location**: `s_doppler_state[MAX_EDICTS]` (line 76) and bounds check (line 1105)

**Problem**: Bounds check uses `cl.csr.max_edicts` but array is sized to `MAX_EDICTS`. If the two ever differ, we could have indexing issues.

**Current Status**: `MAX_EDICTS` is typically 8192 and `cl.csr.max_edicts` is ≤ `MAX_EDICTS`, so this is safe but fragile.

**Fix**: Use `MAX_EDICTS` consistently or dynamically allocate:
```cpp
if (entnum < 0 || entnum >= MAX_EDICTS)
    return false;
```

**Effort**: Trivial (2 minutes)

---

### 7. **Approaching/Receding Detection Missing** 🔧 Improvement

**Location**: Entire Doppler system

**Problem**: No explicit debugging or visualization of whether Doppler is approaching (higher pitch) or receding (lower pitch). This makes tuning difficult.

**Improvement**: Add debug output:
```cpp
#if USE_DEBUG
if (s_show->integer > 2 && VectorLength(velocity) > min_speed) {
    vec3_t to_source;
    VectorSubtract(origin, listener_origin, to_source);
    float dot = DotProduct(velocity, to_source);
    Com_Printf("Doppler ent %d: %s (speed %.0f)\n", 
        entnum, dot < 0 ? "approaching" : "receding", 
        VectorLength(velocity));
}
#endif
```

**Effort**: Low (30 minutes)

---

## Proposed Improvements

### 1. **Relative Velocity Mode** ⭐ High Impact

Currently, OpenAL computes Doppler from absolute source and listener velocities. An alternative is to compute relative velocity directly and pass it as the source velocity with listener velocity zeroed.

**Benefit**: More control over the Doppler calculation, easier to debug.

**Implementation**:
```cpp
// Instead of:
qalSource3f(srcnum, AL_VELOCITY, source_velocity);
qalListener3f(AL_VELOCITY, listener_velocity);

// Use:
vec3_t relative;
VectorSubtract(source_velocity, listener_velocity, relative);
qalSource3f(srcnum, AL_VELOCITY, relative);
qalListener3f(AL_VELOCITY, 0, 0, 0);
```

**Effort**: Medium (2 hours + testing)

---

### 2. **Per-Sound Doppler Override** ⭐ Medium Impact

Some sounds should never have Doppler (UI, music, ambient loops), while others should always have it (vehicles, projectiles). Add per-sound override.

**Implementation**:
```cpp
typedef enum {
    DOPPLER_DEFAULT,    // Use entity flags
    DOPPLER_FORCE_ON,   // Always apply
    DOPPLER_FORCE_OFF   // Never apply
} doppler_mode_t;

// In sfx_t or playsound_t:
doppler_mode_t doppler_mode;
```

**Effort**: High (4-6 hours, requires sound definition changes)

---

### 3. **Doppler Intensity Scaling by Distance** ⭐ Medium Impact

Real-world Doppler is less noticeable at distance due to lower frequencies dominating. Close Doppler sources should have more dramatic pitch shift.

**Implementation**:
```cpp
float distance_factor = 1.0f - Q_clipf(distance / 2048.0f, 0.0f, 0.5f);
// Scale velocity by distance_factor before applying
```

**Effort**: Low (30 minutes)

---

### 4. **Doppler Statistics and Debug Overlay** 🔧 Quality of Life

Add a stat display similar to reverb stats:
```cpp
static void AL_Doppler_stat(void) {
    int active = 0;
    for (int i = 0; i < MAX_EDICTS; i++)
        if (s_doppler_state[i].time > 0) active++;
    
    SCR_StatKeyValue("doppler sources", va("%d", active));
    SCR_StatKeyValue("listener speed", va("%.0f", VectorLength(s_doppler_listener_velocity)));
}
```

**Effort**: Low (1 hour)

---

## Priority Matrix

| # | Issue/Improvement | Impact | Effort | Priority |
|---|-------------------|--------|--------|----------|
| 1 | Teleport velocity spike | Medium | Low | **High** |
| 6 | Array bounds consistency | Low | Trivial | **High** |
| 2 | Listener frame time fallback | Low | Trivial | Medium |
| 7 | Debug approach/recede output | Low | Low | Medium |
| 4 | Relative velocity mode | Medium | Medium | Medium |
| 3 | Distance-based intensity | Medium | Low | Medium |
| 5 | One-shot Doppler | Low | Medium | Low |
| 8 | Per-sound override | Medium | High | Low |

---

## Conclusion

The Doppler implementation is functional and robust for normal gameplay. The highest priority fixes are:

1. **Teleport detection** - Prevent velocity spikes when entities teleport
2. **Array bounds cleanup** - Use consistent `MAX_EDICTS` for safety

Medium-term improvements should focus on:
- Debug tools for tuning (approach/recede indicator, stats overlay)
- Relative velocity mode for better control

The system correctly handles the most common cases (projectiles, player motion) and the existing smoothing/clamping prevents most edge case issues.
