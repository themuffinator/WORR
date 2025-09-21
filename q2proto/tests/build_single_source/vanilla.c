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

#include "vanilla.h"
#include "tests/defs.h"

#define Q2PROTO_ENTITY_STATE_FEATURES Q2PROTO_FEATURES_VANILLA
#define Q2PROTO_PLAYER_STATE_FEATURES Q2PROTO_FEATURES_VANILLA
#define Q2PROTO_PUBLIC_API            static MAYBE_UNUSED
#define Q2PROTO_PRIVATE_API           static MAYBE_UNUSED
#include "single_source_q2proto.c"

#include "tests/types/vanilla.h"

#define Q2P_PACK_ENTITY_FUNCTION_NAME VanillaPackEntity
#define Q2P_PACK_ENTITY_TYPE          vanilla_entity_state_t *

#include "q2proto/q2proto_packing_entitystate_impl.inc"

#define Q2P_PACK_PLAYER_FUNCTION_NAME VanillaPackPlayer
#define Q2P_PACK_PLAYER_TYPE          vanilla_player_state_t *

#include "q2proto/q2proto_packing_playerstate_impl.inc"

void do_vanilla_things(void)
{
    q2proto_clientcontext_t client_context;
    q2proto_init_clientcontext(&client_context);

    q2proto_server_info_t server_info = {0};
    q2proto_connect_t connect_info = {0};
    q2proto_servercontext_t server_context;
    q2proto_init_servercontext(&server_context, &server_info, &connect_info);

    vanilla_entity_state_t ent = {0};
    q2proto_packed_entity_state_t packed_ent;
    VanillaPackEntity(&server_context, &ent, &packed_ent);

    vanilla_player_state_t player = {0};
    q2proto_packed_player_state_t packed_player;
    VanillaPackPlayer(&server_context, &player, &packed_player);
}
