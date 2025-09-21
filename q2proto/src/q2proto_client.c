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
#include "q2proto/q2proto_client.h"

#include "q2proto/q2proto_io.h"
#include "q2proto_internal.h"

#include <stdlib.h>

// Pick "best" accepted protocol from a comma-separated list of protocol versions. Ignore unsupported/invalid versions.
static void parse_challenge_protocol(q2proto_string_t protos_str, const q2proto_protocol_t *accepted_protocols,
                                     size_t num_accepted_protocols, q2proto_challenge_t *parsed_challenge)
{
    size_t best_protocol_idx = SIZE_MAX;
    q2proto_protocol_t best_protocol = Q2P_PROTOCOL_INVALID;

    q2proto_string_t proto_num_token;
    while (next_token(&proto_num_token, &protos_str, ',')) {
        errno = 0;
        long proto_value = q2pstol(&proto_num_token, 10);
        if (errno != 0)
            continue;

        for (size_t i = 0; i < num_accepted_protocols; i++) {
            q2proto_protocol_t p = accepted_protocols[i];
            if (q2proto_get_protocol_netver(p) == proto_value) {
                if (i < best_protocol_idx) {
                    best_protocol_idx = i;
                    best_protocol = p;
                }
                break;
            }
        }
    }
    if (best_protocol != Q2P_PROTOCOL_INVALID)
        parsed_challenge->server_protocol = best_protocol;
}

q2proto_error_t q2proto_parse_challenge(const char *challenge_args, const q2proto_protocol_t *accepted_protocols,
                                        size_t num_accepted_protocols, q2proto_challenge_t *parsed_challenge)
{
    q2proto_string_t challenge_str = q2proto_make_string(challenge_args);

    // Parse challenge value
    q2proto_string_t challenge_value_token;
    if (!next_token(&challenge_value_token, &challenge_str, ' '))
        return Q2P_ERR_BAD_DATA;

    errno = 0;
    long challenge_value = q2pstol(&challenge_value_token, 10);
    if (errno != 0)
        return Q2P_ERR_BAD_DATA;

    parsed_challenge->challenge = (int32_t)challenge_value;
    parsed_challenge->server_protocol = Q2P_PROTOCOL_INVALID;

    // No "p=" args means vanilla protocol. See if it's in the list of accepted protocols.
    for (size_t i = 0; i < num_accepted_protocols; i++) {
        if (accepted_protocols[i] == Q2P_PROTOCOL_VANILLA) {
            parsed_challenge->server_protocol = Q2P_PROTOCOL_INVALID;
            break;
        }
    }

    // Parse challenge args
    q2proto_string_t challenge_arg;
    while (next_token(&challenge_arg, &challenge_str, ' ')) {
        if (strncmp(challenge_arg.str, "p=", 2) == 0) {
            parse_challenge_protocol(q2ps_substr(&challenge_arg, 2), accepted_protocols, num_accepted_protocols,
                                     parsed_challenge);
        }
    }

    return parsed_challenge->server_protocol != Q2P_PROTOCOL_INVALID ? Q2P_ERR_SUCCESS : Q2P_ERR_NO_ACCEPTABLE_PROTOCOL;
}

q2proto_error_t q2proto_complete_connect(q2proto_connect_t *connect)
{
    switch (connect->protocol) {
    case Q2P_PROTOCOL_INVALID:
    case Q2P_PROTOCOL_OLD_DEMO:
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO:
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_V2_DEMO:
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO_PLAYERFOG:
    case Q2P_PROTOCOL_KEX_DEMOS:
    case Q2P_PROTOCOL_KEX:
    case Q2P_NUM_PROTOCOLS:
        // none of these should be used for actual connections
        break;
    case Q2P_PROTOCOL_VANILLA:
        // do nothing
        return Q2P_ERR_SUCCESS;
    case Q2P_PROTOCOL_R1Q2:
        return q2proto_r1q2_complete_connect(connect);
    case Q2P_PROTOCOL_Q2PRO:
        return q2proto_q2pro_complete_connect(connect);
    case Q2P_PROTOCOL_Q2REPRO:
        return q2proto_q2repro_complete_connect(connect);
    }

    return Q2P_ERR_PROTOCOL_NOT_SUPPORTED;
}

q2proto_error_t q2proto_get_connect_arguments(char *args_str, size_t size, size_t *need_size,
                                              const q2proto_connect_t *connect)
{
    int result = Q2P_ERR_SUCCESS;
    size_t buf_needed = 0;

#define SET_ERROR(err)             \
    if (result == Q2P_ERR_SUCCESS) \
    result = err

    int net_protocol = q2proto_get_protocol_netver(connect->protocol);
    if (net_protocol == 0) {
        SET_ERROR(Q2P_ERR_PROTOCOL_NOT_SUPPORTED);
        net_protocol = 9999; // to take up realistic amount of buffer space
    }

#define ADD_FORMAT_IMPL(...)                                   \
    do {                                                       \
        size_t buf_remaining = size;                           \
        int fmt_result = q2proto_snprintf_update(__VA_ARGS__); \
        if (fmt_result < 0) {                                  \
            SET_ERROR(Q2P_ERR_BAD_DATA);                       \
            break;                                             \
        }                                                      \
        buf_needed += fmt_result;                              \
        if (fmt_result >= buf_remaining) {                     \
            SET_ERROR(Q2P_ERR_BUFFER_TOO_SMALL);               \
        }                                                      \
    } while (0)
#if defined(_MSC_VER)
    #define ADD_FORMAT(FMT, ...) ADD_FORMAT_IMPL(&args_str, &size, (FMT), ##__VA_ARGS__)
#else
    #define ADD_FORMAT(FMT, ...) ADD_FORMAT_IMPL(&args_str, &size, (FMT)__VA_OPT__(, ) __VA_ARGS__)
#endif

    // Common part
    ADD_FORMAT("%d %d %d \"", net_protocol, connect->qport, connect->challenge);
    {
        size_t userinfo_len = q2pslcpy(args_str, size, &connect->userinfo);
        if (userinfo_len >= size) {
            SET_ERROR(Q2P_ERR_BUFFER_TOO_SMALL);
            size = 0;
        } else {
            buf_needed += userinfo_len;
            size -= userinfo_len;
            args_str += userinfo_len;
        }
    }

    const char *tail = NULL;
    switch (connect->protocol) {
    case Q2P_PROTOCOL_INVALID:
    case Q2P_PROTOCOL_OLD_DEMO:
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO:
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_V2_DEMO:
    case Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO_PLAYERFOG:
    case Q2P_PROTOCOL_KEX_DEMOS:
    case Q2P_PROTOCOL_KEX:
    case Q2P_NUM_PROTOCOLS:
        // shouldn't be used for "connect"
        SET_ERROR(Q2P_ERR_PROTOCOL_NOT_SUPPORTED);
        break;
    case Q2P_PROTOCOL_VANILLA:
        // no extra connect args
        break;
    case Q2P_PROTOCOL_R1Q2:
        tail = q2proto_r1q2_connect_tail(connect);
        break;
    case Q2P_PROTOCOL_Q2PRO:
        tail = q2proto_q2pro_connect_tail(connect);
        break;
    case Q2P_PROTOCOL_Q2REPRO:
        tail = q2proto_q2repro_connect_tail(connect);
        break;
    }

    if (tail) {
        ADD_FORMAT("\" %s", tail);
    } else {
        ADD_FORMAT("\"");
    }

#undef SET_ERROR
#undef ADD_FORMAT
#undef ADD_FORMAT_IMPL

    if (need_size)
        *need_size = buf_needed + 1;
    return result;
}

static q2proto_error_t default_client_packet_parse(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                   q2proto_svc_message_t *svc_message);
static q2proto_error_t default_client_send(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                           const q2proto_clc_message_t *clc_message);

q2proto_error_t q2proto_init_clientcontext(q2proto_clientcontext_t *context)
{
    memset(context, 0, sizeof(*context));

    context->client_read = default_client_packet_parse;
    context->client_write = default_client_send;

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                    q2proto_svc_message_t *svc_message)
{
    return context->client_read(context, io_arg, svc_message);
}

static MAYBE_UNUSED const char *default_server_cmd_string(int command)
{
    const char *str = q2proto_debug_common_svc_string(command);
    return str ? str : q2proto_va("%d", command);
}

static q2proto_error_t default_client_packet_parse(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                   q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    size_t command_read = 0;
    const void *command_ptr = NULL;
    READ_CHECKED(client_read, io_arg, command_ptr, raw, 1, &command_read);
    if (command_read == 0)
        return Q2P_ERR_NO_MORE_INPUT;

    uint8_t command = *(const uint8_t *)command_ptr;
    SHOWNET(io_arg, 1, -1, "%s", default_server_cmd_string(command));
    if (command == svc_stufftext) {
        svc_message->type = Q2P_SVC_STUFFTEXT;
        q2proto_common_client_read_stufftext(io_arg, &svc_message->stufftext);
        return Q2P_ERR_SUCCESS;
    } else if (command != svc_serverdata)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_EXPECTED_SERVERDATA, "expected svc_serverdata, got %d",
                            command);

    svc_message->type = Q2P_SVC_SERVERDATA;

    int32_t protocol;
    READ_CHECKED(client_read, io_arg, protocol, i32);

    svc_message->serverdata.protocol = protocol;
    switch (protocol) {
    default:
        // Allow a range of versions to support old demos
        if (protocol >= PROTOCOL_OLD_DEMO && protocol <= PROTOCOL_VANILLA)
            return q2proto_vanilla_continue_serverdata(context, io_arg, &svc_message->serverdata);
        break;
    case PROTOCOL_R1Q2:
        return q2proto_r1q2_continue_serverdata(context, io_arg, &svc_message->serverdata);
    case PROTOCOL_Q2PRO:
        return q2proto_q2pro_continue_serverdata(context, io_arg, &svc_message->serverdata);
    case PROTOCOL_Q2PRO_DEMO_EXT:
    case PROTOCOL_Q2PRO_DEMO_EXT_LIMITS_2:
    case PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG:
        return q2proto_q2pro_extdemo_continue_serverdata(context, io_arg, &svc_message->serverdata);
    case PROTOCOL_Q2REPRO:
        return q2proto_q2repro_continue_serverdata(context, io_arg, &svc_message->serverdata);
    case PROTOCOL_KEX_DEMOS:
    case PROTOCOL_KEX:
        return q2proto_kex_continue_serverdata(context, io_arg, &svc_message->serverdata);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_PROTOCOL_NOT_SUPPORTED, "protocol unsupported: %d", protocol);
}

q2proto_error_t q2proto_client_download_reset(q2proto_clientcontext_t *context)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    if (context->has_zdownload_inflate_io_arg) {
        CHECKED_IO(client_read, context->zdownload_inflate_io_arg,
                   q2protoio_inflate_end(context->zdownload_inflate_io_arg), "finishing inflate");
        context->has_zdownload_inflate_io_arg = false;
    }
#endif
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                     const q2proto_clc_message_t *clc_message)
{
    q2proto_clientcontext_t *ctx_internal = (q2proto_clientcontext_t *)context;
    return ctx_internal->client_write(ctx_internal, io_arg, clc_message);
}

uint32_t q2proto_client_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                   const q2proto_vec3_t maxs)
{
    return context->pack_solid(context, mins, maxs);
}

void q2proto_client_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                                 q2proto_vec3_t maxs)
{
    context->unpack_solid(context, solid, mins, maxs);
}

static q2proto_error_t default_client_send(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                           const q2proto_clc_message_t *clc_message)
{
    // stringcmds are sent before serverdata is received, handle this
    if (clc_message->type != Q2P_CLC_STRINGCMD)
        return HANDLE_ERROR(client_write, io_arg, Q2P_ERR_BAD_COMMAND, "unexpected message type %d", clc_message->type);

    return q2proto_common_client_write_stringcmd(io_arg, &clc_message->stringcmd);
}
