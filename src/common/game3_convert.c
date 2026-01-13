/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "common/game3_convert.h"

void ConvertToGame3_pmove_state_old(game3_pmove_state_old_t *game_pmove_state, const pmove_state_t *server_pmove_state, bool extended)
{
    game_pmove_state->pm_type = pmtype_to_game3(server_pmove_state->pm_type);
    VectorScale(server_pmove_state->origin, 8, game_pmove_state->origin);
    VectorScale(server_pmove_state->velocity, 8, game_pmove_state->velocity);
    game_pmove_state->pm_flags = pmflags_to_game3(server_pmove_state->pm_flags, extended);
    game_pmove_state->pm_time = server_pmove_state->pm_time / 8;
    game_pmove_state->gravity = server_pmove_state->gravity;
    game_pmove_state->delta_angles[0] = ANGLE2SHORT(server_pmove_state->delta_angles[0]);
    game_pmove_state->delta_angles[1] = ANGLE2SHORT(server_pmove_state->delta_angles[1]);
    game_pmove_state->delta_angles[2] = ANGLE2SHORT(server_pmove_state->delta_angles[2]);
}

void ConvertFromGame3_pmove_state_old(pmove_state_t *pmove_state, const game3_pmove_state_old_t *game_pmove_state, bool extended)
{
    pmove_state->pm_type = pmtype_from_game3(game_pmove_state->pm_type);
    VectorScale(game_pmove_state->origin, 0.125f, pmove_state->origin);
    VectorScale(game_pmove_state->velocity, 0.125f, pmove_state->velocity);
    pmove_state->pm_flags = pmflags_from_game3(game_pmove_state->pm_flags, extended);
    pmove_state->pm_time = game_pmove_state->pm_time * 8;
    pmove_state->gravity = game_pmove_state->gravity;
    pmove_state->delta_angles[0] = SHORT2ANGLE(game_pmove_state->delta_angles[0]);
    pmove_state->delta_angles[1] = SHORT2ANGLE(game_pmove_state->delta_angles[1]);
    pmove_state->delta_angles[2] = SHORT2ANGLE(game_pmove_state->delta_angles[2]);
}

void ConvertToGame3_pmove_state_new(game3_pmove_state_new_t *game_pmove_state, const pmove_state_t *server_pmove_state, bool extended)
{
    game_pmove_state->pm_type = pmtype_to_game3(server_pmove_state->pm_type);
    VectorScale(server_pmove_state->origin, 8, game_pmove_state->origin);
    VectorScale(server_pmove_state->velocity, 8, game_pmove_state->velocity);
    game_pmove_state->pm_flags = pmflags_to_game3(server_pmove_state->pm_flags, extended);
    game_pmove_state->pm_time = server_pmove_state->pm_time;
    game_pmove_state->gravity = server_pmove_state->gravity;
    game_pmove_state->delta_angles[0] = ANGLE2SHORT(server_pmove_state->delta_angles[0]);
    game_pmove_state->delta_angles[1] = ANGLE2SHORT(server_pmove_state->delta_angles[1]);
    game_pmove_state->delta_angles[2] = ANGLE2SHORT(server_pmove_state->delta_angles[2]);
}

void ConvertFromGame3_pmove_state_new(pmove_state_t *pmove_state, const game3_pmove_state_new_t *game_pmove_state, bool extended)
{
    pmove_state->pm_type = pmtype_from_game3(game_pmove_state->pm_type);
    VectorScale(game_pmove_state->origin, 0.125f, pmove_state->origin);
    VectorScale(game_pmove_state->velocity, 0.125f, pmove_state->velocity);
    pmove_state->pm_flags = pmflags_from_game3(game_pmove_state->pm_flags, extended);
    pmove_state->pm_time = game_pmove_state->pm_time;
    pmove_state->gravity = game_pmove_state->gravity;
    pmove_state->delta_angles[0] = SHORT2ANGLE(game_pmove_state->delta_angles[0]);
    pmove_state->delta_angles[1] = SHORT2ANGLE(game_pmove_state->delta_angles[1]);
    pmove_state->delta_angles[2] = SHORT2ANGLE(game_pmove_state->delta_angles[2]);
}

void ConvertToGame3_usercmd(game3_usercmd_t *game_cmd, const usercmd_t *server_cmd)
{
    game_cmd->msec = server_cmd->msec;
    game_cmd->buttons = server_cmd->buttons;
    game_cmd->angles[0] = ANGLE2SHORT(server_cmd->angles[0]);
    game_cmd->angles[1] = ANGLE2SHORT(server_cmd->angles[1]);
    game_cmd->angles[2] = ANGLE2SHORT(server_cmd->angles[2]);
    game_cmd->forwardmove = server_cmd->forwardmove;
    game_cmd->sidemove = server_cmd->sidemove;
    if(server_cmd->buttons & BUTTON_JUMP)
        game_cmd->upmove = 200;
    else if(server_cmd->buttons & BUTTON_CROUCH)
        game_cmd->upmove = -200;
    else
        game_cmd->upmove = 0;
    game_cmd->impulse = 0;
    game_cmd->lightlevel = 128; // FIXME
}
