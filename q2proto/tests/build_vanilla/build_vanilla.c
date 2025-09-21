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

#include "q2proto/q2proto.h"

#include "tests/types/vanilla.h"

/*
 * Basic test for compile & link checks.
 * Will not do anything sensible!
 */

#define Q2P_PACK_ENTITY_FUNCTION_NAME PackEntity
#define Q2P_PACK_ENTITY_TYPE          vanilla_entity_state_t *

#include "q2proto/q2proto_packing_entitystate_impl.inc"

#define Q2P_PACK_PLAYER_FUNCTION_NAME PackPlayer
#define Q2P_PACK_PLAYER_TYPE          vanilla_player_state_t *

#include "q2proto/q2proto_packing_playerstate_impl.inc"

int main(int argc, char **argv)
{
    q2proto_clientcontext_t client_context;
    q2proto_init_clientcontext(&client_context);

    q2proto_server_info_t server_info = {0};
    q2proto_connect_t connect_info = {0};
    q2proto_servercontext_t server_context;
    q2proto_init_servercontext(&server_context, &server_info, &connect_info);

    vanilla_entity_state_t ent = {0};
    q2proto_packed_entity_state_t packed_ent;
    PackEntity(&server_context, &ent, &packed_ent);

    vanilla_player_state_t player = {0};
    q2proto_packed_player_state_t packed_player;
    PackPlayer(&server_context, &player, &packed_player);

    return 0;
}
