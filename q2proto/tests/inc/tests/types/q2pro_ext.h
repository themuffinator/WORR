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
 * Q2PRO extended game types
 */
#ifndef TESTS_TYPES_Q2PRO_EXT_H_
#define TESTS_TYPES_Q2PRO_EXT_H_

#include "q2proto/q2proto.h"

typedef q2proto_vec3_t vec3_t;

// Based on Q2PRO entity state + extension
typedef struct {
    int number; // edict index

    vec3_t origin;
    vec3_t angles;
    vec3_t old_origin; // for lerping
    int modelindex;
    int modelindex2, modelindex3, modelindex4; // weapons, CTF flags, etc
    int frame;
    int skinnum;
    uint64_t effects; // PGM - we're filling it, so it needs to be unsigned
    int renderfx;
    int solid; // for client side prediction, 8*(bits 0-4) is x/y radius
               // 8*(bits 5-9) is z down distance, 8(bits10-15) is z up
               // gi.linkentity sets this properly
    int sound; // for looping sounds, to guarantee shutoff
    int event; // impulse events -- muzzle flashes, footsteps, etc
               // events only go out for a single frame, they
               // are automatically cleared each frame

    float alpha;
    float scale;
    float loop_volume;
    float loop_attenuation;
} q2pro_ext_entity_state_t;

typedef enum {
    // can accelerate and turn
    PM_NORMAL,
    PM_SPECTATOR,
    // no acceleration or turning
    PM_DEAD,
    PM_GIB, // different bounding box
    PM_FREEZE
} q2pro_ext_pmtype_t;

// OG Quake2 pmove
typedef struct {
    q2pro_ext_pmtype_t pm_type;

    short origin[3];   // 12.3
    short velocity[3]; // 12.3
    uint8_t pm_flags;  // ducked, jump_held, etc
    uint8_t pm_time;   // each unit = 8 ms
    short gravity;
    short delta_angles[3]; // add to command angles to get view direction
                           // changed by spawns, rotating objects, and teleporters
} q2pro_ext_pmove_state_t;

// Slightly modified Q2PRO "old" player state
typedef struct {
    q2pro_ext_pmove_state_t pmove; // for prediction

    // these fields do not need to be communicated bit-precise

    vec3_t viewangles;  // for fixed views
    vec3_t viewoffset;  // add to pmovestate->origin
    vec3_t kick_angles; // add to view direction to get render angles
                        // set by weapon kicks, pain effects, etc

    vec3_t gunangles;
    vec3_t gunoffset;
    int gunindex;
    int gunskin;
    int gunframe;

    float blend[4]; // rgba full screen effect

    float fov; // horizontal field of view

    int rdflags; // refdef flags

    short stats[32]; // fast status bar updates
} q2pro_ext_player_state_t;

#endif // TESTS_TYPES_Q2PRO_EXT_H_
