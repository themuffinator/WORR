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
 * Implementation of functions to read messages written by game
 */
#ifndef Q2PROTO_BUILD
#define Q2PROTO_BUILD
#endif // Q2PROTO_BUILD
#include "q2proto/q2proto.h"

#if !defined(READ_GAME_DIRECTION)
#define READ_GAME_DIRECTION q2proto_common_client_read_packed_direction
#endif

#if !defined(READ_TEMP_ENTITY_DECL)
#define READ_TEMP_ENTITY_DECL
#define _READ_TEMP_ENTITY_DECL_DEFAULTED
#endif

#if !defined(HANDLE_UNKNOWN_TEMP_ENTITY)
#define HANDLE_UNKNOWN_TEMP_ENTITY(IO_ARG, TEMP_ENTITY, RESULT) false
#define _HANDLE_UNKNOWN_TEMP_ENTITY_DEFAULTED
#endif

#define HANDLE_SPECIAL_TEMP_ENT_Q2PRO_EXT _Q2PROTO_CONCAT(READ_TEMP_ENTITY_NAME, _q2pro_ext)

static q2proto_error_t HANDLE_SPECIAL_TEMP_ENT_Q2PRO_EXT(uintptr_t io_arg, q2proto_svc_temp_entity_t *temp_entity,
                                                         q2proto_error_t *result)
{
    if (temp_entity->type != TE_Q2PRO_DAMAGE_DEALT)
        return false;
    READ_CHECKED(client_read, io_arg, temp_entity->count, i16);
    *result = Q2P_ERR_SUCCESS;
    return true;
}

READ_TEMP_ENTITY_DECL q2proto_error_t READ_TEMP_ENTITY_NAME(uintptr_t io_arg, q2proto_game_api_t game_api,
                                                            q2proto_svc_temp_entity_t *temp_entity)
{
    READ_CHECKED(client_read, io_arg, temp_entity->type, u8);

    switch (temp_entity->type) {
    case TE_BLOOD:
    case TE_GUNSHOT:
    case TE_SPARKS:
    case TE_BULLET_SPARKS:
    case TE_SCREEN_SPARKS:
    case TE_SHIELD_SPARKS:
    case TE_SHOTGUN:
    case TE_BLASTER:
    case TE_GREENBLOOD:
    case TE_BLASTER2:
    case TE_FLECHETTE:
    case TE_HEATBEAM_SPARKS:
    case TE_HEATBEAM_STEAM:
    case TE_MOREBLOOD:
    case TE_ELECTRIC_SPARKS:
    case TE_BLUEHYPERBLASTER_2:
    case TE_BERSERK_SLAM:
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_DIRECTION(io_arg, temp_entity->direction));
        break;

    case TE_SPLASH:
    case TE_LASER_SPARKS:
    case TE_WELDING_SPARKS:
    case TE_TUNNEL_SPARKS:
        READ_CHECKED(client_read, io_arg, temp_entity->count, u8);
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_DIRECTION(io_arg, temp_entity->direction));
        READ_CHECKED(client_read, io_arg, temp_entity->color, u8);
        break;

    case TE_BLUEHYPERBLASTER:
    case TE_RAILTRAIL:
    case TE_RAILTRAIL2:
    case TE_BUBBLETRAIL:
    case TE_DEBUGTRAIL:
    case TE_BUBBLETRAIL2:
    case TE_BFG_LASER:
    case TE_BFG_ZAP:
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position2));
        break;

    case TE_GRENADE_EXPLOSION:
    case TE_GRENADE_EXPLOSION_WATER:
    case TE_EXPLOSION2:
    case TE_PLASMA_EXPLOSION:
    case TE_ROCKET_EXPLOSION:
    case TE_ROCKET_EXPLOSION_WATER:
    case TE_EXPLOSION1:
    case TE_EXPLOSION1_NP:
    case TE_EXPLOSION1_BIG:
    case TE_BFG_EXPLOSION:
    case TE_BFG_BIGEXPLOSION:
    case TE_BOSSTPORT:
    case TE_PLAIN_EXPLOSION:
    case TE_CHAINFIST_SMOKE:
    case TE_TRACKER_EXPLOSION:
    case TE_TELEPORT_EFFECT:
    case TE_DBALL_GOAL:
    case TE_WIDOWSPLASH:
    case TE_NUKEBLAST:
    case TE_EXPLOSION1_NL:
    case TE_EXPLOSION2_NL:
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        break;

    case TE_PARASITE_ATTACK:
    case TE_MEDIC_CABLE_ATTACK:
    case TE_HEATBEAM:
    case TE_MONSTER_HEATBEAM:
    case TE_GRAPPLE_CABLE_2:
    case TE_LIGHTNING_BEAM:
        READ_CHECKED(client_read, io_arg, temp_entity->entity1, i16);
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position2));
        break;

    case TE_GRAPPLE_CABLE:
        READ_CHECKED(client_read, io_arg, temp_entity->entity1, i16);
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position2));
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->offset));
        break;

    case TE_LIGHTNING:
        READ_CHECKED(client_read, io_arg, temp_entity->entity1, i16);
        READ_CHECKED(client_read, io_arg, temp_entity->entity2, i16);
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position2));
        break;

    case TE_FLASHLIGHT:
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        READ_CHECKED(client_read, io_arg, temp_entity->entity1, i16);
        break;

    case TE_FORCEWALL:
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position2));
        READ_CHECKED(client_read, io_arg, temp_entity->color, u8);
        break;

    case TE_STEAM:
        READ_CHECKED(client_read, io_arg, temp_entity->entity1, i16);
        READ_CHECKED(client_read, io_arg, temp_entity->count, u8);
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        CHECKED(client_read, io_arg, READ_GAME_DIRECTION(io_arg, temp_entity->direction));
        READ_CHECKED(client_read, io_arg, temp_entity->color, u8);
        READ_CHECKED(client_read, io_arg, temp_entity->entity2, i16);
        if (temp_entity->entity1 != -1) {
            READ_CHECKED(client_read, io_arg, temp_entity->time, i32);
        }
        break;

    case TE_WIDOWBEAMOUT:
        READ_CHECKED(client_read, io_arg, temp_entity->entity1, i16);
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, temp_entity->position1));
        break;

    case TE_POWER_SPLASH:
        READ_CHECKED(client_read, io_arg, temp_entity->entity1, i16);
        READ_CHECKED(client_read, io_arg, temp_entity->count, u8);
        break;

    default:
        {
            q2proto_error_t handle_res = Q2P_ERR_BAD_DATA;
            bool tent_handled = false;
            if (game_api == Q2PROTO_GAME_Q2PRO_EXTENDED || game_api == Q2PROTO_GAME_Q2PRO_EXTENDED_V2)
                tent_handled = HANDLE_SPECIAL_TEMP_ENT_Q2PRO_EXT(io_arg, temp_entity, &handle_res);
            if (tent_handled)
                return handle_res;
            else
                return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "%s: invalid tent %d", __func__,
                                    temp_entity->type);
        }
    }

    return Q2P_ERR_SUCCESS;
}

#if defined(_HANDLE_UNKNOWN_TEMP_ENTITY_DEFAULTED)
#undef _HANDLE_UNKNOWN_TEMP_ENTITY_DEFAULTED
#undef HANDLE_UNKNOWN_TEMP_ENTITY
#endif // _HANDLE_UNKNOWN_TEMP_ENTITY_DEFAULTED

#if defined(_READ_TEMP_ENTITY_DECL_DEFAULTED)
#undef _READ_TEMP_ENTITY_DECL_DEFAULTED
#undef READ_TEMP_ENTITY_DECL
#endif // _READ_TEMP_ENTITY_DECL_DEFAULTED

#undef HANDLE_SPECIAL_TEMP_ENT_Q2PRO_EXT

#if !defined(READ_SOUND_DECL)
#define READ_SOUND_DECL
#define _READ_SOUND_DECL_DEFAULTED
#endif

READ_SOUND_DECL q2proto_error_t READ_SOUND_NAME(uintptr_t io_arg, q2proto_svc_sound_t *sound)
{
    READ_CHECKED(client_read, io_arg, sound->flags, u8);
    if (sound->flags & SND_Q2PRO_INDEX16)
        READ_CHECKED(client_read, io_arg, sound->index, u16);
    else
        READ_CHECKED(client_read, io_arg, sound->index, u8);

    if (sound->flags & SND_VOLUME)
        READ_CHECKED(client_read, io_arg, sound->volume, u8);
    else
        sound->volume = SOUND_DEFAULT_VOLUME;

    if (sound->flags & SND_ATTENUATION)
        READ_CHECKED(client_read, io_arg, sound->attenuation, u8);
    else
        sound->attenuation = SOUND_DEFAULT_ATTENUATION;

    if (sound->flags & SND_OFFSET)
        READ_CHECKED(client_read, io_arg, sound->timeofs, u8);

    if (sound->flags & SND_ENT) {
        // entity relative
        uint16_t channel;
        READ_CHECKED(client_read, io_arg, channel, u16);
        sound->entity = channel >> 3;
        sound->channel = channel & 7;
    }

    // positioned in space
    if (sound->flags & SND_POS) {
        q2proto_vec3_t pos;
        CHECKED(client_read, io_arg, READ_GAME_POSITION(io_arg, pos));
        q2proto_var_coords_set_float(&sound->pos, pos);
    }

    return Q2P_ERR_SUCCESS;
}

#if defined(_READ_SOUND_DECL_DEFAULTED)
#undef _READ_TEMP_ENTITY_DECL_DEFAULTED
#undef READ_SOUND_DECL
#endif // _READ_SOUND_DECL_DEFAULTED
