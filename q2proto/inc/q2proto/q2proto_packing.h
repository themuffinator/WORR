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
 * Packed representations of entities, player states
 */
#ifndef Q2PROTO_STRUCT_PACKING_H_
#define Q2PROTO_STRUCT_PACKING_H_

#include "q2proto_defs.h"
#include "q2proto_game_api.h"
#include "q2proto_limits.h" // for Q2PROTO_STATS

/// Packed representation of entity state. Use with only server context used for packing!
typedef struct q2proto_packed_entity_state_s {
    uint16_t modelindex;
    uint16_t modelindex2;
    uint16_t modelindex3;
    uint16_t modelindex4;
    uint16_t frame;
    uint32_t skinnum;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    uint64_t effects;
#else
    uint32_t effects;
#endif
    uint32_t renderfx;
    int32_t origin[3];
    int16_t angles[3];
    int32_t old_origin[3];
    uint16_t sound;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    uint8_t loop_volume;
    uint8_t loop_attenuation;
#endif
    uint8_t event;
    uint32_t solid;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    uint8_t alpha;
    uint8_t scale;
#endif
} q2proto_packed_entity_state_t;

/// Packed representation of player state. Use with only server context used for packing!
typedef struct q2proto_packed_player_state_s {
    uint8_t pm_type;
    int32_t pm_origin[3];
    int32_t pm_velocity[3];
    uint16_t pm_time;
    uint16_t pm_flags;
    int16_t pm_gravity;
    int16_t pm_delta_angles[3];
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
    int8_t pm_viewheight;
#endif
    int16_t viewoffset[3];
    int16_t viewangles[3];
    int16_t kick_angles[3];
    uint16_t gunindex;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    uint8_t gunskin;
#endif
    uint16_t gunframe;
    int16_t gunoffset[3];
    int16_t gunangles[3];
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
    uint8_t gunrate;
#endif
    uint8_t blend[4];
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    uint8_t damage_blend[4];
#endif
    uint8_t fov;
    uint8_t rdflags;
    int16_t stats[Q2PROTO_STATS];
#if Q2PROTO_PLAYER_STATE_FEATURES == Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    uint8_t fog_color[3];
    uint16_t fog_density;
    uint16_t fog_skyfactor;
    uint8_t heightfog_start_color[3];
    uint8_t heightfog_end_color[3];
    uint16_t heightfog_density;
    uint16_t heightfog_falloff;
    int32_t heightfog_start_dist;
    int32_t heightfog_end_dist;
#endif
} q2proto_packed_player_state_t;

/**\def Q2PROTO_DECLARE_ENTITY_PACKING_FUNCTION
 * Declare a function to pack an entity state in a protocol-dependent manner.
 *
 * Packed entity states can be used to later fill an "entity state delta" message, saving on data conversions required
 * by the protocol. Packing function arguments:
 * - \c context Server communications context.
 * - \c entity_state Entity state to create a packed representation of.
 * - \c entity_packed Receives the packed entity state representation.
 *
 * To define this function include `q2proto_packing_entitystate_impl.inc`.
 */
#define Q2PROTO_DECLARE_ENTITY_PACKING_FUNCTION(FUNCTION_NAME, ENTITYSTATE_TYPE)              \
    void FUNCTION_NAME(q2proto_servercontext_t *context, const ENTITYSTATE_TYPE entity_state, \
                       q2proto_packed_entity_state_t *entity_packed)

/**\def Q2PROTO_DECLARE_PLAYER_PACKING_FUNCTION
 * Declare a function to pack a player state in a protocol-dependent manner.
 * To define this function include `q2proto_packing_playerstate_impl.inc`.

 * Packed player states can be used to later fill an "player state delta" message, saving on data conversions required
 by the protocol.
 * Packing function arguments:
 * - \c context Server communications context.
 * - \c player_state Player state to create a packed representation of.
 * - \c player_packed Receives the player entity state representation.
 */
#define Q2PROTO_DECLARE_PLAYER_PACKING_FUNCTION(FUNCTION_NAME, PLAYERSTATE_TYPE)              \
    void FUNCTION_NAME(q2proto_servercontext_t *context, const PLAYERSTATE_TYPE player_state, \
                       q2proto_packed_player_state_t *player_packed)

/**\name Internal packing support
 * @{ */
typedef struct q2proto_servercontext_s q2proto_servercontext_t;

// Entity, player packing flavor to use, depending on server context
typedef enum _q2proto_packing_flavor_e {
    // Pack for vanilla (and derived)
    _Q2P_PACKING_VANILLA = 0,
    // Pack for q2repro protocol
    _Q2P_PACKING_REPRO = 1,
} _q2proto_packing_flavor_t;

// Call actual entity packing function
Q2PROTO_PUBLIC_API _q2proto_packing_flavor_t _q2proto_get_packing_flavor(q2proto_servercontext_t *context,
                                                                         q2proto_game_api_t *game_api);
/** @} */

#endif // Q2PROTO_STRUCT_PACKING_H_
