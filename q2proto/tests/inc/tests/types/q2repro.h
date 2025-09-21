/*
Copyright (C) 2024 Frank Richter

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/**\file
 * Rerelease game types
 */
#ifndef TESTS_TYPES_Q2REPRO_H_
#define TESTS_TYPES_Q2REPRO_H_

#include "q2proto/q2proto.h"

typedef q2proto_vec3_t vec3_t;

// Based on rerelease game entity_state_t
typedef struct {
    uint32_t number; // edict index

    vec3_t origin;
    vec3_t angles;
    vec3_t old_origin; // for lerping
    int32_t modelindex;
    int32_t modelindex2, modelindex3, modelindex4; // weapons, CTF flags, etc
    int32_t frame;
    int32_t skinnum;
    uint64_t effects; // PGM - we're filling it, so it needs to be unsigned
    uint32_t renderfx;
    uint32_t solid;        // for client side prediction
    int32_t sound;         // for looping sounds, to guarantee shutoff
    uint8_t event;         // impulse events -- muzzle flashes, footsteps, etc
                           // events only go out for a single frame, they
                           // are automatically cleared each frame
    float alpha;           // [Paril-KEX] alpha scalar; 0 is a "default" value, which will respect other
                           // settings (default 1.0 for most things, EF_TRANSLUCENT will default this
                           // to 0.3, etc)
    float scale;           // [Paril-KEX] model scale scalar; 0 is a "default" value, like with alpha.
    uint8_t instance_bits; // [Paril-KEX] players that *can't* see this entity will have a bit of 1. handled by
                           // the server, do not set directly.
    // [Paril-KEX] allow specifying volume/attn for looping noises; note that
    // zero will be defaults (1.0 and 3.0 respectively); -1 attenuation is used
    // for "none" (similar to target_speaker) for no phs/pvs looping noises
    float loop_volume;
    float loop_attenuation;
    // [Paril-KEX] for proper client-side owner collision skipping
    int32_t owner;
    // [Paril-KEX] for custom interpolation stuff
    int32_t old_frame;
} q2repro_entity_state_t;

typedef enum {
    // can accelerate and turn
    PM_NORMAL,
    PM_GRAPPLE, // [Paril-KEX] pull towards velocity, no gravity
    PM_NOCLIP,
    PM_SPECTATOR, // [Paril-KEX] clip against walls, but not entities
    // no acceleration or turning
    PM_DEAD,
    PM_GIB, // different bounding box
    PM_FREEZE
} q2repro_pmtype_t;

// Based on rerelease game pmove
typedef struct {
    q2repro_pmtype_t pm_type;

    vec3_t origin;
    vec3_t velocity;
    uint16_t pm_flags; // ducked, jump_held, etc
    uint16_t pm_time;
    int16_t gravity;
    vec3_t delta_angles; // add to command angles to get view direction
                         // changed by spawns, rotating objects, and teleporters
    int8_t viewheight;   // view height, added to origin[2] + viewoffset[2], for crouching
} q2repro_pmove_state_t;

// Based on rerelease game player state
typedef struct {
    q2repro_pmove_state_t pmove; // for prediction

    // these fields do not need to be communicated bit-precise

    vec3_t viewangles;  // for fixed views
    vec3_t viewoffset;  // add to pmovestate->origin
    vec3_t kick_angles; // add to view direction to get render angles
                        // set by weapon kicks, pain effects, etc

    vec3_t gunangles;
    vec3_t gunoffset;
    int32_t gunindex;
    int32_t gunskin; // [Paril-KEX] gun skin #
    int32_t gunframe;
    int32_t gunrate; // [Paril-KEX] tickrate of gun animations; 0 and 10 are equivalent

    float blend[4];        // rgba full screen effect
    float damage_blend[4]; // [Paril-KEX] rgba damage blend effect

    float fov; // horizontal field of view

    uint8_t rdflags; // refdef flags

    int16_t stats[64]; // fast status bar updates

    uint8_t team_id; // team identifier
} q2repro_player_state_t;

#endif // TESTS_TYPES_Q2REPRO_H_
