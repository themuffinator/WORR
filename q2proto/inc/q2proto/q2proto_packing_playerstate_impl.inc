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
 * Provide implementation of a player state packing function.
 * To generate a packing function, the following macros \em must be define beforehand:
 * - #Q2P_PACK_PLAYER_FUNCTION_NAME
 * - #Q2P_PACK_PLAYER_TYPE
 * The following macros can be defined to customize retrieval of player state fields:
 * - #Q2P_PACK_GET_PLAYER_VALUE
 * - #Q2P_PACK_PLAYER_STATS_NUM
 * - #Q2P_PACK_GET_PLAYER_PMOVE_VALUE
 * - #Q2P_PACK_GET_PLAYER_FOG_VALUE
 * - #Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE
 */
#include "q2proto.h"

#include "q2proto_packing_helpers.h"

/**\def Q2P_PACK_ENTITY_FUNCTION_NAME
 * Name of generated player state packing function.
 */
#if !defined(Q2P_PACK_PLAYER_FUNCTION_NAME)
    #error Please define Q2P_PACK_PLAYER_FUNCTION_NAME.
#endif

/**\def Q2P_PACK_PLAYER_TYPE
 * Player state type to pack.
 */
#if !defined(Q2P_PACK_PLAYER_TYPE)
    #error Please define Q2P_PACK_PLAYER_TYPE.
#endif

// Prototype to avoid "no previous prototype" warnings
void Q2P_PACK_PLAYER_FUNCTION_NAME(q2proto_servercontext_t *context, const Q2P_PACK_PLAYER_TYPE player_state,
                                   q2proto_packed_player_state_t *player_packed);

/**\def Q2P_PACK_GET_PLAYER_VALUE
 * Extract a member from player state packing input data.
 */
#if !defined(Q2P_PACK_GET_PLAYER_VALUE)
    #define Q2P_PACK_GET_PLAYER_VALUE(PLAYER, MEMBER) ((PLAYER)->MEMBER)
    #define _Q2P_PACK_GET_PLAYER_VALUE_DEFAULTED
#endif // !defined(Q2P_PACK_GET_PLAYER_VALUE)

/**\def Q2P_PACK_PLAYER_STATS_NUM
 * Get number of stats fields in player state
 */
#if !defined(Q2P_PACK_PLAYER_STATS_NUM)
    #define Q2P_PACK_PLAYER_STATS_NUM(PLAYER) 32 // conservative default
    #define _Q2P_PACK_PLAYER_STATS_NUM_DEFAULTED
#endif // !defined(Q2P_PACK_PLAYER_STATS_NUM)

/**\def Q2P_PACK_GET_PLAYER_PMOVE_VALUE
 * Extract a member from pmove data contained in player state packing input data.
 */
#if !defined(Q2P_PACK_GET_PLAYER_PMOVE_VALUE)
    #define Q2P_PACK_GET_PLAYER_PMOVE_VALUE(PLAYER, PM_MEMBER) ((PLAYER)->pmove.PM_MEMBER)
    #define _Q2P_PACK_GET_PLAYER_PMOVE_VALUE_DEFAULTED
#endif // !defined(Q2P_PACK_GET_PLAYER_VALUE)

/**\def Q2P_PACK_GET_PLAYER_FOG_VALUE
 * Extract a member from fog data contained in player state packing input data.
 */
#if !defined(Q2P_PACK_GET_PLAYER_FOG_VALUE)
    #define Q2P_PACK_GET_PLAYER_FOG_VALUE(PLAYER, FOG_MEMBER) ((PLAYER)->fog.FOG_MEMBER)
    #define _Q2P_PACK_GET_PLAYER_FOG_VALUE_DEFAULTED
#endif // !defined(Q2P_PACK_GET_PLAYER_FOG_VALUE)

/**\def Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE
 * Extract a member from heightfog data contained in player state packing input data.
 */
#if !defined(Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE)
    #define Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE(PLAYER, HF_MEMBER) ((PLAYER)->heightfog.HF_MEMBER)
    #define _Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE_DEFAULTED
#endif // !defined(Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE)

#define _Q2P_PACK_PLAYER_VANILLA_FUNCTION_NAME _Q2PROTO_PACKING_NAME(Q2P_PACK_PLAYER_FUNCTION_NAME, _int)
#define _Q2P_PACK_PLAYER_Q2REPRO_FUNCTION_NAME _Q2PROTO_PACKING_NAME(Q2P_PACK_PLAYER_FUNCTION_NAME, _float)

// Pack a player state a for Vanilla, R1Q2, Q2PRO, Q2PRO extended - they're relatively similar and can be handled with a
// single function.
static void _Q2P_PACK_PLAYER_VANILLA_FUNCTION_NAME(const Q2P_PACK_PLAYER_TYPE restrict player_state,
                                                   q2proto_packed_player_state_t *restrict player_packed)
{
    memset(player_packed, 0, sizeof(*player_packed));

    player_packed->pm_type = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, pm_type);
    _Q2P_PACKING_PACK_COORD_VEC_TO_INT(player_packed->pm_origin, Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, origin));
    _Q2P_PACKING_PACK_COORD_VEC_TO_INT(player_packed->pm_velocity,
                                       Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, velocity));
    player_packed->pm_time = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, pm_time);
    player_packed->pm_flags = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, pm_flags);
    player_packed->pm_gravity = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, gravity);
    _Q2P_PACKING_PACK_ANGLE_VEC_TO_INT(player_packed->pm_delta_angles,
                                       Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, delta_angles));
    const float *viewoffset = Q2P_PACK_GET_PLAYER_VALUE(player_state, viewoffset);
    player_packed->viewoffset[0] = _q2proto_valenc_smalloffset2char(viewoffset[0]);
    player_packed->viewoffset[1] = _q2proto_valenc_smalloffset2char(viewoffset[1]);
    player_packed->viewoffset[2] = _q2proto_valenc_smalloffset2char(viewoffset[2]);
    const float *viewangles = Q2P_PACK_GET_PLAYER_VALUE(player_state, viewangles);
    player_packed->viewangles[0] = _q2proto_valenc_angle2short(viewangles[0]);
    player_packed->viewangles[1] = _q2proto_valenc_angle2short(viewangles[1]);
    player_packed->viewangles[2] = _q2proto_valenc_angle2short(viewangles[2]);
    const float *kick_angles = Q2P_PACK_GET_PLAYER_VALUE(player_state, kick_angles);
    player_packed->kick_angles[0] = _q2proto_valenc_smallangle2char(kick_angles[0]);
    player_packed->kick_angles[1] = _q2proto_valenc_smallangle2char(kick_angles[1]);
    player_packed->kick_angles[2] = _q2proto_valenc_smallangle2char(kick_angles[2]);
    player_packed->gunindex = (int16_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, gunindex);
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    player_packed->gunskin = (int8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, gunskin);
#endif
    player_packed->gunframe = (uint8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, gunframe);
    const float *gunoffset = Q2P_PACK_GET_PLAYER_VALUE(player_state, gunoffset);
    player_packed->gunoffset[0] = _q2proto_valenc_smalloffset2char(gunoffset[0]);
    player_packed->gunoffset[1] = _q2proto_valenc_smalloffset2char(gunoffset[1]);
    player_packed->gunoffset[2] = _q2proto_valenc_smalloffset2char(gunoffset[2]);
    const float *gunangles = Q2P_PACK_GET_PLAYER_VALUE(player_state, gunangles);
    player_packed->gunangles[0] = _q2proto_valenc_smallangle2char(gunangles[0]);
    player_packed->gunangles[1] = _q2proto_valenc_smallangle2char(gunangles[1]);
    player_packed->gunangles[2] = _q2proto_valenc_smallangle2char(gunangles[2]);
    const float *blend = Q2P_PACK_GET_PLAYER_VALUE(player_state, blend);
    player_packed->blend[0] = _q2proto_valenc_color2byte(blend[0]);
    player_packed->blend[1] = _q2proto_valenc_color2byte(blend[1]);
    player_packed->blend[2] = _q2proto_valenc_color2byte(blend[2]);
    player_packed->blend[3] = _q2proto_valenc_color2byte(blend[3]);
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    const float *damage_blend = Q2P_PACK_GET_PLAYER_VALUE(player_state, damage_blend);
    player_packed->damage_blend[0] = _q2proto_valenc_color2byte(damage_blend[0]);
    player_packed->damage_blend[1] = _q2proto_valenc_color2byte(damage_blend[1]);
    player_packed->damage_blend[2] = _q2proto_valenc_color2byte(damage_blend[2]);
    player_packed->damage_blend[3] = _q2proto_valenc_color2byte(damage_blend[3]);
#endif
    player_packed->fov = (uint8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, fov);
    player_packed->rdflags = (uint8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, rdflags);
    const int16_t *stats = Q2P_PACK_GET_PLAYER_VALUE(player_state, stats);
    memcpy(&player_packed->stats, stats, sizeof(int16_t) * Q2P_PACK_PLAYER_STATS_NUM(player_state));
#if Q2PROTO_PLAYER_STATE_FEATURES == Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    const float *fog_color = Q2P_PACK_GET_PLAYER_FOG_VALUE(player_state, color);
    player_packed->fog_color[0] = _q2proto_valenc_color2byte(fog_color[0]);
    player_packed->fog_color[1] = _q2proto_valenc_color2byte(fog_color[1]);
    player_packed->fog_color[2] = _q2proto_valenc_color2byte(fog_color[2]);
    player_packed->fog_density =
        _q2proto_valenc_clamped_mul(Q2P_PACK_GET_PLAYER_FOG_VALUE(player_state, density), UINT16_MAX, 0, UINT16_MAX);
    player_packed->fog_skyfactor =
        _q2proto_valenc_clamped_mul(Q2P_PACK_GET_PLAYER_FOG_VALUE(player_state, sky_factor), UINT16_MAX, 0, UINT16_MAX);
    const float *heightfog_start_color = Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE(player_state, start_color);
    player_packed->heightfog_start_color[0] = _q2proto_valenc_color2byte(heightfog_start_color[0]);
    player_packed->heightfog_start_color[1] = _q2proto_valenc_color2byte(heightfog_start_color[1]);
    player_packed->heightfog_start_color[2] = _q2proto_valenc_color2byte(heightfog_start_color[2]);
    const float *heightfog_end_color = Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE(player_state, end_color);
    player_packed->heightfog_end_color[0] = _q2proto_valenc_color2byte(heightfog_end_color[0]);
    player_packed->heightfog_end_color[1] = _q2proto_valenc_color2byte(heightfog_end_color[1]);
    player_packed->heightfog_end_color[2] = _q2proto_valenc_color2byte(heightfog_end_color[2]);
    player_packed->heightfog_density = _q2proto_valenc_clamped_mul(
        Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE(player_state, density), UINT16_MAX, 0, UINT16_MAX);
    player_packed->heightfog_falloff = _q2proto_valenc_clamped_mul(
        Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE(player_state, falloff), UINT16_MAX, 0, UINT16_MAX);
    player_packed->heightfog_start_dist =
        _q2proto_valenc_coord2int(Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE(player_state, start_dist));
    player_packed->heightfog_end_dist =
        _q2proto_valenc_coord2int(Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE(player_state, end_dist));
#endif
}

static void _Q2P_PACK_PLAYER_Q2REPRO_FUNCTION_NAME(const Q2P_PACK_PLAYER_TYPE restrict player_state,
                                                   q2proto_packed_player_state_t *restrict player_packed)
{
    memset(player_packed, 0, sizeof(*player_packed));

    player_packed->pm_type = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, pm_type);
    _Q2P_PACKING_PACK_COORD_VEC_TO_FLOATBITS(player_packed->pm_origin,
                                             Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, origin));
    _Q2P_PACKING_PACK_COORD_VEC_TO_FLOATBITS(player_packed->pm_velocity,
                                             Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, velocity));
    player_packed->pm_time = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, pm_time);
    player_packed->pm_flags = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, pm_flags);
    player_packed->pm_gravity = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, gravity);
    _Q2P_PACKING_PACK_ANGLE_VEC_TO_INT(player_packed->pm_delta_angles,
                                       Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, delta_angles));
    const float *viewoffset = Q2P_PACK_GET_PLAYER_VALUE(player_state, viewoffset);
    player_packed->viewoffset[0] = _q2proto_valenc_q2repro_viewoffset2short(viewoffset[0]);
    player_packed->viewoffset[1] = _q2proto_valenc_q2repro_viewoffset2short(viewoffset[1]);
    player_packed->viewoffset[2] = _q2proto_valenc_q2repro_viewoffset2short(viewoffset[2]);
    const float *viewangles = Q2P_PACK_GET_PLAYER_VALUE(player_state, viewangles);
    player_packed->viewangles[0] = _q2proto_valenc_angle2short(viewangles[0]);
    player_packed->viewangles[1] = _q2proto_valenc_angle2short(viewangles[1]);
    player_packed->viewangles[2] = _q2proto_valenc_angle2short(viewangles[2]);
    const float *kick_angles = Q2P_PACK_GET_PLAYER_VALUE(player_state, kick_angles);
    player_packed->kick_angles[0] = _q2proto_valenc_q2repro_kick_angle2short(kick_angles[0]);
    player_packed->kick_angles[1] = _q2proto_valenc_q2repro_kick_angle2short(kick_angles[1]);
    player_packed->kick_angles[2] = _q2proto_valenc_q2repro_kick_angle2short(kick_angles[2]);
    player_packed->gunindex = (int16_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, gunindex);
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    player_packed->gunskin = (int8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, gunskin);
#endif
    player_packed->gunframe = (uint8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, gunframe);
    const float *gunoffset = Q2P_PACK_GET_PLAYER_VALUE(player_state, gunoffset);
    player_packed->gunoffset[0] = _q2proto_valenc_q2repro_gunoffset2short(gunoffset[0]);
    player_packed->gunoffset[1] = _q2proto_valenc_q2repro_gunoffset2short(gunoffset[1]);
    player_packed->gunoffset[2] = _q2proto_valenc_q2repro_gunoffset2short(gunoffset[2]);
    const float *gunangles = Q2P_PACK_GET_PLAYER_VALUE(player_state, gunangles);
    player_packed->gunangles[0] = _q2proto_valenc_q2repro_gunangle2short(gunangles[0]);
    player_packed->gunangles[1] = _q2proto_valenc_q2repro_gunangle2short(gunangles[1]);
    player_packed->gunangles[2] = _q2proto_valenc_q2repro_gunangle2short(gunangles[2]);
    const float *blend = Q2P_PACK_GET_PLAYER_VALUE(player_state, blend);
    player_packed->blend[0] = _q2proto_valenc_color2byte(blend[0]);
    player_packed->blend[1] = _q2proto_valenc_color2byte(blend[1]);
    player_packed->blend[2] = _q2proto_valenc_color2byte(blend[2]);
    player_packed->blend[3] = _q2proto_valenc_color2byte(blend[3]);
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    const float *damage_blend = Q2P_PACK_GET_PLAYER_VALUE(player_state, damage_blend);
    player_packed->damage_blend[0] = _q2proto_valenc_color2byte(damage_blend[0]);
    player_packed->damage_blend[1] = _q2proto_valenc_color2byte(damage_blend[1]);
    player_packed->damage_blend[2] = _q2proto_valenc_color2byte(damage_blend[2]);
    player_packed->damage_blend[3] = _q2proto_valenc_color2byte(damage_blend[3]);
#endif
    player_packed->fov = (uint8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, fov);
    player_packed->rdflags = (uint8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, rdflags);
    const int16_t *stats = Q2P_PACK_GET_PLAYER_VALUE(player_state, stats);
    memcpy(&player_packed->stats, stats, sizeof(int16_t) * Q2P_PACK_PLAYER_STATS_NUM(player_state));
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
    player_packed->gunrate = (int8_t)Q2P_PACK_GET_PLAYER_VALUE(player_state, gunrate);
    player_packed->pm_viewheight = Q2P_PACK_GET_PLAYER_PMOVE_VALUE(player_state, viewheight);
#endif
}

void Q2P_PACK_PLAYER_FUNCTION_NAME(q2proto_servercontext_t *context, const Q2P_PACK_PLAYER_TYPE player_state,
                                   q2proto_packed_player_state_t *player_packed)
{
    switch (_q2proto_get_packing_flavor(context, NULL)) {
    case _Q2P_PACKING_VANILLA:
        // Fall through to default case
        break;
    case _Q2P_PACKING_REPRO:
        _Q2P_PACK_PLAYER_Q2REPRO_FUNCTION_NAME(player_state, player_packed);
        return;
    }
    _Q2P_PACK_PLAYER_VANILLA_FUNCTION_NAME(player_state, player_packed);
}


#undef _Q2P_PACK_PLAYER_VANILLA_FUNCTION_NAME
#undef _Q2P_PACK_PLAYER_Q2REPRO_FUNCTION_NAME

#if defined(_Q2P_PACK_GET_PLAYER_VALUE_DEFAULTED)
    #undef _Q2P_PACK_GET_PLAYER_VALUE_DEFAULTED
    #undef Q2P_PACK_GET_PLAYER_VALUE
#endif // defined(_Q2P_PACK_GET_PLAYER_VALUE_DEFAULTED)

#if defined(_Q2P_PACK_PLAYER_STATS_NUM_DEFAULTED)
    #undef _Q2P_PACK_PLAYER_STATS_NUM_DEFAULTED
    #undef Q2P_PACK_PLAYER_STATS_NUM
#endif // defined(_Q2P_PACK_PLAYER_STATS_NUM_DEFAULTED)

#if defined(_Q2P_PACK_GET_PLAYER_PMOVE_VALUE_DEFAULTED)
    #undef _Q2P_PACK_GET_PLAYER_PMOVE_VALUE_DEFAULTED
    #undef Q2P_PACK_GET_PLAYER_PMOVE_VALUE
#endif // defined(_Q2P_PACK_GET_PLAYER_PMOVE_VALUE_DEFAULTED)

#if defined(_Q2P_PACK_GET_PLAYER_FOG_VALUE_DEFAULTED)
    #undef _Q2P_PACK_GET_PLAYER_FOG_VALUE_DEFAULTED
    #undef Q2P_PACK_GET_PLAYER_FOG_VALUE
#endif // defined(_Q2P_PACK_GET_PLAYER_FOG_VALUE_DEFAULTED)

#if defined(_Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE_DEFAULTED)
    #undef _Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE_DEFAULTED
    #undef Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE
#endif // defined(_Q2P_PACK_GET_PLAYER_HEIGHTFOG_VALUE_DEFAULTED)
