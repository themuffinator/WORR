/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003-2024 Andrey Nazarov
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

#include "shared/shared.h"
#include "common/q2proto_shared.h"

#define Q2P_PACK_ENTITY_FUNCTION_NAME    PackEntity
#define Q2P_PACK_ENTITY_TYPE             entity_state_t *

#include "q2proto/q2proto_packing_entitystate_impl.inc"

// player_state_t lacks some fields, so use wrapper functions here
#define Q2P_PACK_GET_PLAYER_VALUE(PLAYER, MEMBER)   (Q2P_PlayerState_##MEMBER(PLAYER))

static inline const float* Q2P_PlayerState_viewangles(const player_state_t *ps)
{
    return ps->viewangles;
}

static inline const float* Q2P_PlayerState_viewoffset(const player_state_t *ps)
{
    return ps->viewoffset;
}

static inline const float* Q2P_PlayerState_kick_angles(const player_state_t *ps)
{
    return ps->kick_angles;
}

static inline const float* Q2P_PlayerState_gunangles(const player_state_t *ps)
{
    return ps->gunangles;
}

static inline const float* Q2P_PlayerState_gunoffset(const player_state_t *ps)
{
    return ps->gunoffset;
}

static inline int Q2P_PlayerState_gunindex(const player_state_t *ps)
{
    return ps->gunindex;
}

static inline int Q2P_PlayerState_gunskin(const player_state_t *ps)
{
    return ps->gunskin;
}

static inline int Q2P_PlayerState_gunframe(const player_state_t *ps)
{
    return ps->gunframe;
}

static inline const float* Q2P_PlayerState_blend(const player_state_t *ps)
{
    return ps->screen_blend;
}

static inline const float* Q2P_PlayerState_damage_blend(const player_state_t *ps)
{
    return ps->damage_blend;
}

static inline int Q2P_PlayerState_fov(const player_state_t *ps)
{
    return ps->fov;
}

static inline int Q2P_PlayerState_rdflags(const player_state_t *ps)
{
    return ps->rdflags;
}

static inline const short* Q2P_PlayerState_stats(const player_state_t *ps)
{
    return ps->stats;
}

static inline int Q2P_PlayerState_gunrate(const player_state_t *ps)
{
    return ps->gunrate;
}

#define Q2P_PACK_PLAYER_FUNCTION_NAME    PackPlayerstate
#define Q2P_PACK_PLAYER_TYPE             player_state_t*
#define Q2P_PACK_PLAYER_STATS_NUM(PLAYER)       MAX_STATS

#include "q2proto/q2proto_packing_playerstate_impl.inc"
