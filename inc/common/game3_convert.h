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

#pragma once

#include "shared/shared.h"
#include "shared/game3_shared.h"

//
// Conversions from/to game3 types
//

// Convert pmove_state_old_t to game3_pmove_state_old_t
void ConvertToGame3_pmove_state_old(game3_pmove_state_old_t *game_pmove_state, const pmove_state_t *server_pmove_state, bool extended);
// Convert game3_pmove_state_t to pmove_state_t
void ConvertFromGame3_pmove_state_old(pmove_state_t *pmove_state, const game3_pmove_state_old_t *game_pmove_state, bool extended);

// Convert pmove_state_t to game3_pmove_state_new_t
void ConvertToGame3_pmove_state_new(game3_pmove_state_new_t *game_pmove_state, const pmove_state_t *server_pmove_state, bool extended);
// Convert game3_pmove_state_t to pmove_state_t
void ConvertFromGame3_pmove_state_new(pmove_state_t *pmove_state, const game3_pmove_state_new_t *game_pmove_state, bool extended);

// Convert usercmd_t to game3_usercmd_t
void ConvertToGame3_usercmd(game3_usercmd_t *game_cmd, const usercmd_t *server_cmd);
