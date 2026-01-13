/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003-2011 Richard Stanway
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

#define Q2PROTO_BUILD
#include "q2proto/q2proto_protocol.h"

#include "q2proto_internal_protocol.h"

int q2proto_get_protocol_netver(q2proto_protocol_t protocol)
{
    switch (protocol) {
    case Q2P_PROTOCOL_INVALID:
    case Q2P_NUM_PROTOCOLS:
        break;
    case Q2P_PROTOCOL_OLD_DEMO:
        return PROTOCOL_OLD_DEMO;
    case Q2P_PROTOCOL_VANILLA:
        return PROTOCOL_VANILLA;
    case Q2P_PROTOCOL_R1Q2:
        return PROTOCOL_R1Q2;
    case Q2P_PROTOCOL_Q2PRO:
        return PROTOCOL_Q2PRO;
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO:
        return PROTOCOL_Q2PRO_DEMO_EXT;
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_V2_DEMO:
        return PROTOCOL_Q2PRO_DEMO_EXT_LIMITS_2;
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO_PLAYERFOG:
        return PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG;
    case Q2P_PROTOCOL_Q2REPRO:
        return PROTOCOL_Q2REPRO;
    case Q2P_PROTOCOL_KEX_DEMOS:
        return PROTOCOL_KEX_DEMOS;
    case Q2P_PROTOCOL_KEX:
        return PROTOCOL_KEX;
    }

    return 0;
}

q2proto_protocol_t q2proto_protocol_from_netver(int version)
{
    switch (version) {
    case PROTOCOL_OLD_DEMO:
        return Q2P_PROTOCOL_OLD_DEMO;
    case PROTOCOL_VANILLA:
        return Q2P_PROTOCOL_VANILLA;
    case PROTOCOL_R1Q2:
        return Q2P_PROTOCOL_R1Q2;
    case PROTOCOL_Q2PRO:
        return Q2P_PROTOCOL_Q2PRO;
    case PROTOCOL_Q2PRO_DEMO_EXT:
        return Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO;
    case PROTOCOL_Q2PRO_DEMO_EXT_LIMITS_2:
        return Q2P_PROTOCOL_Q2PRO_EXTENDED_V2_DEMO;
    case PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG:
        return Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO_PLAYERFOG;
    case PROTOCOL_Q2REPRO:
        return Q2P_PROTOCOL_Q2REPRO;
    case PROTOCOL_KEX_DEMOS:
        return Q2P_PROTOCOL_KEX_DEMOS;
    case PROTOCOL_KEX:
        return Q2P_PROTOCOL_KEX;
    }

    return Q2P_PROTOCOL_INVALID;
}

static inline unsigned int get_proto_mask_for_game(q2proto_game_api_t game)
{
    switch (game) {
    case Q2PROTO_GAME_VANILLA:
        return BIT(Q2P_PROTOCOL_VANILLA) | BIT(Q2P_PROTOCOL_R1Q2) | BIT(Q2P_PROTOCOL_Q2PRO);
    case Q2PROTO_GAME_Q2PRO_EXTENDED:
        return BIT(Q2P_PROTOCOL_Q2PRO);
    case Q2PROTO_GAME_Q2PRO_EXTENDED_V2:
        return BIT(Q2P_PROTOCOL_Q2PRO);
    case Q2PROTO_GAME_RERELEASE:
        return BIT(Q2P_PROTOCOL_Q2REPRO);
    }

    return 0;
}

size_t q2proto_get_protocols_for_gametypes(q2proto_protocol_t *protocols, size_t num_protocols,
                                           const q2proto_game_api_t *games, size_t num_games)
{
    unsigned int proto_mask = 0;
    for (size_t i = 0; i < num_games; i++) {
        proto_mask |= get_proto_mask_for_game(games[i]);
    }

    size_t n = 0;
    for (int i = Q2P_NUM_PROTOCOLS; i-- > 0;) {
        if ((proto_mask & BIT(i)) == 0)
            continue;
        if (num_protocols > 0) {
            *protocols++ = (q2proto_protocol_t)i;
            --num_protocols;
            n++;
        }
    }
    return n;
}

q2proto_multicast_protocol_t q2proto_get_multicast_protocol(q2proto_protocol_t *protocols, size_t num_protocols,
                                                            q2proto_game_api_t game_api)
{
    unsigned int proto_bits = 0;
    for (size_t i = 0; i < num_protocols; i++) {
        proto_bits |= BIT(protocols[i]);
    }

    unsigned protocols_supported = get_proto_mask_for_game(game_api);
    unsigned protocols_unsupported = ~protocols_supported;

    if ((proto_bits & protocols_supported) == 0 || (proto_bits & protocols_unsupported) != 0)
        goto fail;

    switch (game_api) {
    case Q2PROTO_GAME_VANILLA:
    case Q2PROTO_GAME_Q2PRO_EXTENDED:
        return Q2P_PROTOCOL_MULTICAST_SHORT;
    case Q2PROTO_GAME_Q2PRO_EXTENDED_V2:
        return Q2P_PROTOCOL_MULTICAST_Q2PRO_EXT;
    case Q2PROTO_GAME_RERELEASE:
        return Q2P_PROTOCOL_MULTICAST_FLOAT;
    }

fail:
    return Q2P_PROTOCOL_MULTICAST_INVALID;
}
