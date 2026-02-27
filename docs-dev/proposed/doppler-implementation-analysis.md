# Doppler Effect Implementation Analysis

## Executive Summary

The WORR Doppler implementation has **critical issues** causing projectiles with the Doppler flag to have excessively loud and broken sound. The root causes are: missing source state initialization causing sounds to play in incorrect spatialization modes, and volume stacking from bypassed loop merging.

---

## Critical Issues (Causing Loud/Broken Sound)

### 1. **Missing `AL_SOURCE_RELATIVE` Initialization** 🐛 CRITICAL

**Location**: `AL_PlayChannel()` (lines 1267-1304)

**Problem**: When `AL_PlayChannel` sets up a new source, it never explicitly sets `AL_SOURCE_RELATIVE` to `AL_FALSE`. OpenAL sources can be recycled, and if the previous use had `AL_SOURCE_RELATIVE = AL_TRUE` (as merged sounds do at line 1575), the recycled source will **still be relative**.

**Effect**: A relative source ignores world position and plays at the listener's location at full volume. This explains the "excessively loud" symptom.

**Evidence**:
- Line 1575 (merged sounds): `qalSourcei(ch->srcnum, AL_SOURCE_RELATIVE, AL_TRUE);`
- Line 1206 (in `AL_Spatialize`): Only sets relative mode IF `ch->fullvolume` changed
- `AL_PlayChannel`: Never explicitly initializes relative mode

**Fix**:
```cpp
// In AL_PlayChannel, after line 1283:
qalSourcei(ch->srcnum, AL_SOURCE_RELATIVE, AL_FALSE);
```

**Effort**: Trivial (2 minutes)

---

### 2. **Volume Stacking from Doppler Bypass** 🐛 HIGH

**Location**: `AL_MergeLoopSounds()` (lines 1434-1442)

**Problem**: When any entity in a sound group has `RF_DOPPLER` or Doppler effect flags, **ALL entities with that sound** bypass merging and create individual sources (line 1439). Each source plays at full entity volume.

**Effect**: 5 rockets in the air = 5 separate sources = 5x the effective loudness vs 1 merged source.

**Current Code**:
```cpp
if (has_doppler) {
    for (j = i; j < cl.frame.numEntities; j++) {
        if (sounds[j] != sounds[i])
            continue;
        num = (cl.frame.firstEntity + j) & PARSE_ENTITIES_MASK;
        AL_AddLoopSoundEntity(&cl.entityStates[num], sfx, sc, true);
        sounds[j] = 0;
    }
    continue;
}
```

**Fix Options**:

**Option A** (Simple): Apply volume scaling to Doppler sources:
```cpp
// In AL_AddLoopSoundEntity or AL_Spatialize for no_merge sources:
// Count active sources with same sfx and apply 1/sqrt(count) scaling
```

**Option B** (Better): Only bypass merge for entities that actually have Doppler, not all entities with the same sound:
```cpp
if (has_doppler) {
    for (j = i; j < cl.frame.numEntities; j++) {
        if (sounds[j] != sounds[i])
            continue;
        num = (cl.frame.firstEntity + j) & PARSE_ENTITIES_MASK;
        const entity_state_t *ent_j = &cl.entityStates[num];
        // Only add individually if THIS entity has doppler
        bool ent_doppler = AL_EntityHasDoppler(ent_j);
        if (ent_doppler) {
            AL_AddLoopSoundEntity(ent_j, sfx, sc, true);
        }
        // Non-doppler entities with same sound should still merge
        // (handled in normal merge path below)
        if (ent_doppler)
            sounds[j] = 0;
    }
    // Don't continue - let remaining sounds merge normally
}
```

**Effort**: Medium (1-2 hours)

---

### 3. **`fullvolume` Flag Check Order** 🐛 MEDIUM

**Location**: `AL_Spatialize()` (lines 1201-1213)

**Problem**: Line 1292 in `AL_PlayChannel` sets `ch->fullvolume = -1` to force an update, but the check on line 1202 compares `ch->fullvolume != fullvolume` where `fullvolume` (the computed boolean) will be 0 or 1, not -1. This works by accident, but if the source was previously fullvolume (-1 != 0) it will correctly update.

However, the issue is that `AL_SOURCE_RELATIVE` is only set inside this conditional, meaning if the source is recycled from a relative source AND `fullvolume` happens to match, the relative mode won't be corrected.

**Fix**: Always set `AL_SOURCE_RELATIVE` on first spatialize, or initialize it in `AL_PlayChannel`.

---

## Current Implementation Overview

### How Doppler Sources Are Created

```
AL_MergeLoopSounds()
  └── Check if any entity has doppler → AL_EntityHasDoppler()
      └── If yes: for ALL entities with same sound:
          └── AL_AddLoopSoundEntity(ent, sfx, sc, no_merge=true)
              └── S_PickChannel() → AL_PlayChannel()
                  └── AL_Spatialize()  ← no_merge bypasses early return
                      └── AL_UpdateSourceVelocity() ← applies doppler velocity
```

### How Merged Sources Are Created

```
AL_MergeLoopSounds()
  └── Accumulate volume from all entities into left_total/right_total
  └── Create one source with combined gain/position
  └── qalSourcei(AL_SOURCE_RELATIVE, AL_TRUE)  ← explicit relative mode
  └── qalSource3f(AL_POSITION, pan, 0, pan2)   ← custom positioning
  └── qalSourcef(AL_ROLLOFF_FACTOR, 0.0f)      ← disable distance rolloff
```

### Key Difference

| Property | Doppler Sources | Merged Sources |
|----------|-----------------|----------------|
| Source count | 1 per entity | 1 for all |
| `AL_SOURCE_RELATIVE` | **NOT SET** (bug) | `AL_TRUE` |
| Position mode | World coordinates | Manual pan |
| Volume | 1.0 per source | Combined |
| `AL_ROLLOFF_FACTOR` | `AL_GetRolloffFactor()` | 0.0 |

---

## Other Issues (Lower Priority)

### 4. **Teleport Velocity Spike** ⚠️ Medium

**Location**: `AL_GetEntityVelocity` (lines 1127-1132)

**Problem**: If an entity teleports, the 250ms threshold may not catch it, causing extreme velocity → extreme pitch shift.

**Fix**: Add distance-based discontinuity detection.

---

### 5. **Listener Frame Time Fallback** ⚠️ Low

**Location**: `AL_UpdateListenerVelocity` (line 1790)

**Problem**: If `cls.frametime <= 0`, smoothing lerp factor becomes 1.0, causing instant velocity changes.

---

## Proposed Fixes - Priority Order

| # | Fix | Impact | Effort | Priority |
|---|-----|--------|--------|----------|
| 1 | Add `AL_SOURCE_RELATIVE = FALSE` to `AL_PlayChannel` | Critical | Trivial | **IMMEDIATE** |
| 2 | Apply volume scaling to Doppler sources or only split actual Doppler entities | High | Medium | **HIGH** |
| 3 | Initialize all source state in `AL_PlayChannel` | Medium | Low | Medium |
| 4 | Teleport detection | Low | Low | Low |

---

## Recommended Immediate Fix

```cpp
// In AL_PlayChannel(), after line 1283 (AL_ROLLOFF_FACTOR):

// Ensure source is in absolute world positioning mode
// (critical: recycled sources may have AL_SOURCE_RELATIVE=TRUE from merged sounds)
qalSourcei(ch->srcnum, AL_SOURCE_RELATIVE, AL_FALSE);
```

This one-line fix should resolve the "excessively loud" symptom by ensuring Doppler sources use world-space positioning correctly.

---

## Testing Recommendations

1. **Volume Test**: Fire multiple rockets, observe combined loudness vs single rocket
2. **Spatialization Test**: Stand still, fire rocket past yourself, verify pitch shift AND positional audio (left-to-right panning)
3. **Source Recycling Test**: Play merged sound (ambient loop), then fire rocket - the rocket should NOT play at listener position

---

## Conclusion

The primary cause of the "excessively loud and broken sound" is **missing source state initialization** - specifically `AL_SOURCE_RELATIVE` not being set to `AL_FALSE`. This causes recycled sources to play at the listener's position instead of their world position.

Secondary contribution comes from **volume stacking** when multiple Doppler sources play simultaneously without the volume normalization that merged sources receive.

Both issues have straightforward fixes, with the `AL_SOURCE_RELATIVE` initialization being the critical immediate fix.
