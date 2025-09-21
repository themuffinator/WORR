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
#include "q2proto_internal.h"

/* Q2PRO extended demo protocol is a bit weird,
 * as it mixes stuff from vanilla and Q2PRO protocols... */

//
// CLIENT: PARSE MESSAGES FROM SERVER
//

static q2proto_error_t q2pro_extdemo_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 q2proto_svc_message_t *svc_message);
static q2proto_error_t q2pro_extdemo_client_next_frame_entity_delta(
    q2proto_clientcontext_t *context, uintptr_t io_arg, q2proto_svc_frame_entity_delta_t *frame_entity_delta);
static uint32_t q2pro_extdemo_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                         const q2proto_vec3_t maxs);
static void q2pro_extdemo_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                                       q2proto_vec3_t maxs);

q2proto_error_t q2proto_q2pro_extdemo_continue_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                          q2proto_svc_serverdata_t *serverdata)
{
    bool has_q2pro_extensions_v2 = serverdata->protocol >= PROTOCOL_Q2PRO_DEMO_EXT_LIMITS_2;
    bool has_playerfog = serverdata->protocol >= PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG;

    context->pack_solid = q2pro_extdemo_pack_solid;
    context->unpack_solid = q2pro_extdemo_unpack_solid;

    READ_CHECKED(client_read, io_arg, serverdata->servercount, i32);
    READ_CHECKED(client_read, io_arg, serverdata->attractloop, bool);
    READ_CHECKED(client_read, io_arg, serverdata->gamedir, string);
    READ_CHECKED(client_read, io_arg, serverdata->clientnum, i16);
    READ_CHECKED(client_read, io_arg, serverdata->levelname, string);
    // Extended demo header is essentially a vanilla header

    if (has_playerfog)
        serverdata->protocol_version = PROTOCOL_VERSION_Q2PRO_PLAYERFOG;
    else if (has_q2pro_extensions_v2)
        serverdata->protocol_version = PROTOCOL_VERSION_Q2PRO_EXTENDED_LIMITS_2;
    else
        serverdata->protocol_version = PROTOCOL_VERSION_Q2PRO_EXTENDED_LIMITS;
    serverdata->q2pro.server_state = 2; // ss_game
    serverdata->q2pro.extensions = true;
    serverdata->q2pro.extensions_v2 = has_q2pro_extensions_v2;

    context->client_read = q2pro_extdemo_client_read;
    context->server_protocol = serverdata->protocol;
    context->protocol_version = serverdata->protocol_version;
    context->features.batch_move = true;
    context->features.userinfo_delta = true;
    context->features.has_upmove = true;
    context->features.has_clientnum = true;
    context->features.has_solid32 = true;
    context->features.has_playerfog = has_playerfog;
    context->features.server_game_api =
        has_q2pro_extensions_v2 ? Q2PROTO_GAME_Q2PRO_EXTENDED_V2 : Q2PROTO_GAME_Q2PRO_EXTENDED;

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                            q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t q2pro_extdemo_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                          q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t q2pro_extdemo_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                       uint8_t extrabits, q2proto_svc_frame_t *frame);

static q2proto_error_t q2pro_extdemo_client_read(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                 q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    // zpacket might contain multiple packets, so try to read from inflated message repeatedly
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    size_t command_read = 0;
    const void *command_ptr = NULL;
    READ_CHECKED(client_read, io_arg, command_ptr, raw, 1, &command_read);
    if (command_read == 0) {
#if Q2PROTO_COMPRESSION_DEFLATE
        if (context->has_inflate_io_arg) {
            context->has_inflate_io_arg = false;
            CHECKED_IO(client_read, raw_io_arg, q2protoio_inflate_end(context->inflate_io_arg), "finishing inflate");
            // Call recursively to pick up next message from raw message
            return q2pro_extdemo_client_read(context, raw_io_arg, svc_message);
        }
#endif
        return Q2P_ERR_NO_MORE_INPUT;
    }

    uint8_t command = *(const uint8_t *)command_ptr;
    uint8_t extrabits = command & 0xE0;
    command &= 0x1F;
    SHOWNET(io_arg, 1, -1, "%s", q2proto_debug_common_svc_string(command));

    switch (command) {
    case svc_nop:
        svc_message->type = Q2P_SVC_NOP;
        return Q2P_ERR_SUCCESS;

    case svc_disconnect:
        svc_message->type = Q2P_SVC_DISCONNECT;
        return Q2P_ERR_SUCCESS;

    case svc_reconnect:
        svc_message->type = Q2P_SVC_RECONNECT;
        return Q2P_ERR_SUCCESS;

    case svc_print:
        svc_message->type = Q2P_SVC_PRINT;
        return q2proto_common_client_read_print(io_arg, &svc_message->print);

    case svc_centerprint:
        svc_message->type = Q2P_SVC_CENTERPRINT;
        return q2proto_common_client_read_centerprint(io_arg, &svc_message->centerprint);

    case svc_stufftext:
        svc_message->type = Q2P_SVC_STUFFTEXT;
        return q2proto_common_client_read_stufftext(io_arg, &svc_message->stufftext);

    case svc_serverdata:
        svc_message->type = Q2P_SVC_SERVERDATA;
        return q2pro_extdemo_client_read_serverdata(context, io_arg, &svc_message->serverdata);

    case svc_configstring:
        svc_message->type = Q2P_SVC_CONFIGSTRING;
        return q2proto_common_client_read_configstring(io_arg, &svc_message->configstring);

    case svc_sound:
        svc_message->type = Q2P_SVC_SOUND;
        return q2proto_q2pro_client_read_sound(context, io_arg, &svc_message->sound);

    case svc_spawnbaseline:
        svc_message->type = Q2P_SVC_SPAWNBASELINE;
        return q2pro_extdemo_client_read_baseline(context, io_arg, &svc_message->spawnbaseline);

    case svc_temp_entity:
        svc_message->type = Q2P_SVC_TEMP_ENTITY;
        return q2proto_q2pro_client_read_temp_entity(context, io_arg, &svc_message->temp_entity);

    case svc_muzzleflash:
        svc_message->type = Q2P_SVC_MUZZLEFLASH;
        return q2proto_common_client_read_muzzleflash(io_arg, &svc_message->muzzleflash, MZ_SILENCED);

    case svc_muzzleflash2:
        svc_message->type = Q2P_SVC_MUZZLEFLASH2;
        return q2proto_common_client_read_muzzleflash(io_arg, &svc_message->muzzleflash, 0);

    case svc_download:
        svc_message->type = Q2P_SVC_DOWNLOAD;
        return q2proto_common_client_read_download(io_arg, &svc_message->download);

    case svc_frame:
        svc_message->type = Q2P_SVC_FRAME;
        return q2pro_extdemo_client_read_frame(context, io_arg, extrabits, &svc_message->frame);

    case svc_inventory:
        svc_message->type = Q2P_SVC_INVENTORY;
        return q2proto_common_client_read_inventory(io_arg, &svc_message->inventory);

    case svc_layout:
        svc_message->type = Q2P_SVC_LAYOUT;
        return q2proto_common_client_read_layout(io_arg, &svc_message->layout);

    case svc_r1q2_setting:
        svc_message->type = Q2P_SVC_SETTING;
        return r1q2_client_read_setting(io_arg, &svc_message->setting);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t q2pro_extdemo_client_read_delta_entities(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                                q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    // zpacket might contain multiple packets, so try to read from inflated message repeatedly
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    svc_message->type = Q2P_SVC_FRAME_ENTITY_DELTA;
    q2proto_error_t err =
        q2pro_extdemo_client_next_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);
    if (err != Q2P_ERR_SUCCESS) {
        // FIXME: May be insufficient, might need some explicit way to reset parsing...
        context->client_read = q2pro_extdemo_client_read;
        return err;
    }

    if (svc_message->frame_entity_delta.newnum == 0) {
        context->client_read = q2pro_extdemo_client_read;
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_client_next_frame_entity_delta(
    q2proto_clientcontext_t *context, uintptr_t io_arg, q2proto_svc_frame_entity_delta_t *frame_entity_delta)
{
    memset(frame_entity_delta, 0, sizeof(*frame_entity_delta));

    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &frame_entity_delta->newnum));

    q2proto_debug_shownet_entity_delta_bits(io_arg, "   delta:", frame_entity_delta->newnum, bits);

    if (frame_entity_delta->newnum == 0) {
        return Q2P_ERR_SUCCESS;
    }

    if (bits & U_REMOVE) {
        frame_entity_delta->remove = true;
        return Q2P_ERR_SUCCESS;
    }

    return q2proto_q2pro_client_read_entity_delta(context, io_arg, bits, &frame_entity_delta->entity_delta);
}

static q2proto_error_t q2pro_extdemo_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                            q2proto_svc_serverdata_t *serverdata)
{
    int32_t protocol;
    READ_CHECKED(client_read, io_arg, protocol, i32);

    if (protocol < PROTOCOL_Q2PRO_DEMO_EXT || protocol > PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "unexpected protocol %d", protocol);

    serverdata->protocol = protocol;

    return q2proto_q2pro_extdemo_continue_serverdata(context, io_arg, serverdata);
}

static q2proto_error_t q2pro_extdemo_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                          q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &spawnbaseline->entnum));

    q2proto_debug_shownet_entity_delta_bits(io_arg, "   baseline:", spawnbaseline->entnum, bits);

    return q2proto_q2pro_client_read_entity_delta(context, io_arg, bits, &spawnbaseline->delta_state);
}

static q2proto_error_t q2pro_extdemo_client_read_playerstate(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                             q2proto_svc_playerstate_t *playerstate)
{
    bool has_q2pro_extensions_v2 = context->features.server_game_api == Q2PROTO_GAME_Q2PRO_EXTENDED_V2;
    bool has_playerfog = context->server_protocol >= PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG;
    uint32_t flags;
    READ_CHECKED(client_read, io_arg, flags, u16);
    if (has_playerfog && flags & PS_MOREBITS) {
        uint8_t more_flags;
        READ_CHECKED(client_read, io_arg, more_flags, u8);
        flags |= more_flags << 16;
    }

#if Q2PROTO_SHOWNET
    if (q2protodbg_shownet_check(io_arg, 2) && flags) {
        char buf[1024];
        q2proto_q2pro_debug_player_delta_bits_to_str(buf, sizeof(buf), flags);
        SHOWNET(io_arg, 2, -2, "   %s", buf);
    }
#endif

    //
    // parse the pmove_state_t
    //
    if (delta_bits_check(flags, PS_M_TYPE, &playerstate->delta_bits, Q2P_PSD_PM_TYPE))
        READ_CHECKED(client_read, io_arg, playerstate->pm_type, u8);

    if (flags & PS_M_ORIGIN) {
        CHECKED(client_read, io_arg, client_read_maybe_diff_coords_comp(context, io_arg, &playerstate->pm_origin, 0));
        CHECKED(client_read, io_arg, client_read_maybe_diff_coords_comp(context, io_arg, &playerstate->pm_origin, 1));
        CHECKED(client_read, io_arg, client_read_maybe_diff_coords_comp(context, io_arg, &playerstate->pm_origin, 2));
        playerstate->pm_origin.read.value.delta_bits = 0x7;
    }

    if (flags & PS_M_VELOCITY) {
        CHECKED(client_read, io_arg, client_read_maybe_diff_coords_comp(context, io_arg, &playerstate->pm_velocity, 0));
        CHECKED(client_read, io_arg, client_read_maybe_diff_coords_comp(context, io_arg, &playerstate->pm_velocity, 1));
        CHECKED(client_read, io_arg, client_read_maybe_diff_coords_comp(context, io_arg, &playerstate->pm_velocity, 2));
        playerstate->pm_velocity.read.value.delta_bits = 0x7;
    }

    if (delta_bits_check(flags, PS_M_TIME, &playerstate->delta_bits, Q2P_PSD_PM_TIME)) {
        if (has_q2pro_extensions_v2)
            READ_CHECKED(client_read, io_arg, playerstate->pm_time, u16);
        else
            READ_CHECKED(client_read, io_arg, playerstate->pm_time, u8);
    }

    if (delta_bits_check(flags, PS_M_FLAGS, &playerstate->delta_bits, Q2P_PSD_PM_FLAGS)) {
        if (has_q2pro_extensions_v2)
            READ_CHECKED(client_read, io_arg, playerstate->pm_flags, u16);
        else
            READ_CHECKED(client_read, io_arg, playerstate->pm_flags, u8);
    }

    if (delta_bits_check(flags, PS_M_GRAVITY, &playerstate->delta_bits, Q2P_PSD_PM_GRAVITY))
        READ_CHECKED(client_read, io_arg, playerstate->pm_gravity, i16);

    if (delta_bits_check(flags, PS_M_DELTA_ANGLES, &playerstate->delta_bits, Q2P_PSD_PM_DELTA_ANGLES))
        CHECKED(client_read, io_arg, read_var_angles16(io_arg, &playerstate->pm_delta_angles));

    //
    // parse the rest of the player_state_t
    //
    if (delta_bits_check(flags, PS_VIEWOFFSET, &playerstate->delta_bits, Q2P_PSD_VIEWOFFSET))
        CHECKED(client_read, io_arg, read_var_small_offsets(io_arg, &playerstate->viewoffset));

    if (flags & PS_VIEWANGLES) {
        CHECKED(client_read, io_arg, read_var_angles16(io_arg, &playerstate->viewangles.values));
        playerstate->viewangles.delta_bits = 0x7;
    }

    if (delta_bits_check(flags, PS_KICKANGLES, &playerstate->delta_bits, Q2P_PSD_KICKANGLES))
        CHECKED(client_read, io_arg, read_var_small_angles(io_arg, &playerstate->kick_angles));

    if (delta_bits_check(flags, PS_WEAPONINDEX, &playerstate->delta_bits, Q2P_PSD_GUNINDEX)) {
        uint16_t gun_index_and_skin;
        READ_CHECKED(client_read, io_arg, gun_index_and_skin, u16);
        playerstate->gunindex = gun_index_and_skin & Q2PRO_GUNINDEX_MASK;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        playerstate->gunskin = gun_index_and_skin >> Q2PRO_GUNINDEX_BITS;
#endif
    }

    if (delta_bits_check(flags, PS_WEAPONFRAME, &playerstate->delta_bits, Q2P_PSD_GUNFRAME)) {
        READ_CHECKED(client_read, io_arg, playerstate->gunframe, u8);
        CHECKED(client_read, io_arg, read_var_small_offsets(io_arg, &playerstate->gunoffset.values));
        playerstate->gunoffset.delta_bits = BIT(0) | BIT(1) | BIT(2);
        CHECKED(client_read, io_arg, read_var_small_angles(io_arg, &playerstate->gunangles.values));
        playerstate->gunangles.delta_bits = BIT(0) | BIT(1) | BIT(2);
    }

    if (flags & PS_BLEND) {
        if (has_q2pro_extensions_v2) {
            q2proto_color_delta_t damage_blend = {0};
            CHECKED(client_read, io_arg, client_read_q2pro_extv2_blends(io_arg, &playerstate->blend, &damage_blend));
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
            memcpy(&playerstate->damage_blend, &damage_blend, sizeof(damage_blend));
#endif
        } else {
            CHECKED(client_read, io_arg, read_var_color(io_arg, &playerstate->blend.values));
            playerstate->blend.delta_bits = 0xf;
        }
    }

    if (has_playerfog && flags & PS_Q2PRO_PLAYERFOG) {
        q2proto_svc_fog_t fog = {0};
        CHECKED(client_read, io_arg, q2proto_q2pro_client_read_playerfog(context, io_arg, &fog));
#if Q2PROTO_PLAYER_STATE_FEATURES == Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
        playerstate->fog = fog;
#endif
    }

    if (delta_bits_check(flags, PS_FOV, &playerstate->delta_bits, Q2P_PSD_FOV))
        READ_CHECKED(client_read, io_arg, playerstate->fov, u8);

    if (delta_bits_check(flags, PS_RDFLAGS, &playerstate->delta_bits, Q2P_PSD_RDFLAGS))
        READ_CHECKED(client_read, io_arg, playerstate->rdflags, u8);

    // parse stats
    int numstats = 32;
    if (has_q2pro_extensions_v2) {
        READ_CHECKED(client_read, io_arg, playerstate->statbits, var_u64);
        numstats = 64;
    } else
        READ_CHECKED(client_read, io_arg, playerstate->statbits, u32);
    for (int i = 0; i < numstats; i++)
        if (playerstate->statbits & BIT_ULL(i))
            READ_CHECKED(client_read, io_arg, playerstate->stats[i], i16);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                       uint8_t extrabits, q2proto_svc_frame_t *frame)
{
    READ_CHECKED(client_read, io_arg, frame->serverframe, i32);
    READ_CHECKED(client_read, io_arg, frame->deltaframe, i32);
    READ_CHECKED(client_read, io_arg, frame->suppress_count, u8);

    // read areabits
    READ_CHECKED(client_read, io_arg, frame->areabits_len, u8);
    READ_CHECKED(client_read, io_arg, frame->areabits, raw, frame->areabits_len, NULL);

    uint8_t cmd;
    // read playerinfo
    READ_CHECKED(client_read, io_arg, cmd, u8);
    if (cmd != svc_playerinfo)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "%s: expected playerinfo, got %d", __func__, cmd);
    SHOWNET(io_arg, 2, -1, "playerinfo");
    CHECKED(client_read, io_arg, q2pro_extdemo_client_read_playerstate(context, io_arg, &frame->playerstate));

    // read packet entities
    READ_CHECKED(client_read, io_arg, cmd, u8);
    if (cmd != svc_packetentities)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "%s: expected packetentities, got %d", __func__,
                            cmd);
    context->client_read = q2pro_extdemo_client_read_delta_entities;
    SHOWNET(io_arg, 2, -1, "packetentities");

    return Q2P_ERR_SUCCESS;
}

static uint32_t q2pro_extdemo_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                         const q2proto_vec3_t maxs)
{
    return q2proto_pack_solid_32_q2pro_v2(mins, maxs);
}

static void q2pro_extdemo_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                                       q2proto_vec3_t maxs)
{
    q2proto_unpack_solid_32_q2pro_v2(solid, mins, maxs);
}

//
// SERVER: INITIALIZATION
//

static q2proto_error_t q2pro_extdemo_server_fill_serverdata(q2proto_servercontext_t *context,
                                                            q2proto_svc_serverdata_t *serverdata);
static void q2pro_extdemo_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                         const q2proto_packed_entity_state_t *from,
                                                         const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                         q2proto_entity_state_delta_t *delta);
static void q2pro_extdemo_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                         const q2proto_packed_player_state_t *from,
                                                         const q2proto_packed_player_state_t *to,
                                                         q2proto_svc_playerstate_t *delta);
static q2proto_error_t q2pro_extdemo_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                  const q2proto_svc_message_t *svc_message);
static q2proto_error_t q2pro_extdemo_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                        const q2proto_svc_frame_t *frame);
static q2proto_error_t q2pro_extdemo_server_write_frame_entity_delta(
    q2proto_servercontext_t *context, uintptr_t io_arg, const q2proto_svc_frame_entity_delta_t *frame_entity_delta);
static q2proto_error_t q2pro_extdemo_server_write_gamestate(q2proto_servercontext_t *context,
                                                            q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                            const q2proto_gamestate_t *gamestate);

q2proto_error_t q2proto_q2pro_extdemo_init_servercontext(q2proto_servercontext_t *context,
                                                         const q2proto_connect_t *connect_info)
{
    // Protocol version for compatibility
    context->protocol_version = context->server_info->game_api == Q2PROTO_GAME_Q2PRO_EXTENDED_V2
                                    ? PROTOCOL_VERSION_Q2PRO_CURRENT
                                    : PROTOCOL_VERSION_Q2PRO_EXTENDED_LIMITS;
    context->features.enable_deflate = connect_info->has_zlib;
    context->features.download_compress_raw =
        context->features.enable_deflate && context->protocol_version >= PROTOCOL_VERSION_Q2PRO_ZLIB_DOWNLOADS;
    context->features.has_beam_old_origin_fix = context->protocol_version >= PROTOCOL_VERSION_Q2PRO_BEAM_ORIGIN;
    context->features.playerstate_clientnum = true;
    context->features.has_playerfog = context->protocol >= PROTOCOL_Q2PRO_DEMO_EXT_PLAYERFOG;

    context->fill_serverdata = q2pro_extdemo_server_fill_serverdata;
    context->make_entity_state_delta = q2pro_extdemo_server_make_entity_state_delta;
    context->make_player_state_delta = q2pro_extdemo_server_make_player_state_delta;
    context->server_write = q2pro_extdemo_server_write;
    context->server_write_gamestate = q2pro_extdemo_server_write_gamestate;
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_server_fill_serverdata(q2proto_servercontext_t *context,
                                                            q2proto_svc_serverdata_t *serverdata)
{
    if (context->server_info->game_api == Q2PROTO_GAME_Q2PRO_EXTENDED_V2)
        serverdata->protocol = PROTOCOL_Q2PRO_DEMO_EXT_CURRENT;
    else
        serverdata->protocol = PROTOCOL_Q2PRO_DEMO_EXT;
    serverdata->protocol_version = context->protocol_version;
    return Q2P_ERR_SUCCESS;
}

static void q2pro_extdemo_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                         const q2proto_packed_entity_state_t *from,
                                                         const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                         q2proto_entity_state_delta_t *delta)
{
    q2proto_packing_make_entity_state_delta(from, to, write_old_origin,
                                            context->server_info->game_api != Q2PROTO_GAME_VANILLA, delta);
}

static void q2pro_extdemo_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                         const q2proto_packed_player_state_t *from,
                                                         const q2proto_packed_player_state_t *to,
                                                         q2proto_svc_playerstate_t *delta)
{
    q2proto_packing_make_player_state_delta(from, to, delta);
}

static q2proto_error_t q2pro_extdemo_server_write_serverdata(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                             const q2proto_svc_serverdata_t *serverdata);

static q2proto_error_t q2pro_extdemo_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                  const q2proto_svc_message_t *svc_message)
{
    switch (svc_message->type) {
    case Q2P_SVC_NOP:
        return q2proto_common_server_write_nop(io_arg);

    case Q2P_SVC_DISCONNECT:
        return q2proto_common_server_write_disconnect(io_arg);

    case Q2P_SVC_RECONNECT:
        return q2proto_common_server_write_reconnect(io_arg);

    case Q2P_SVC_SOUND:
        return q2proto_q2pro_server_write_sound(context->protocol, context->server_info, io_arg, &svc_message->sound);

    case Q2P_SVC_PRINT:
        return q2proto_common_server_write_print(io_arg, &svc_message->print);

    case Q2P_SVC_STUFFTEXT:
        return q2proto_common_server_write_stufftext(io_arg, &svc_message->stufftext);

    case Q2P_SVC_SERVERDATA:
        return q2pro_extdemo_server_write_serverdata(context, io_arg, &svc_message->serverdata);

    case Q2P_SVC_CONFIGSTRING:
        return q2proto_common_server_write_configstring(io_arg, &svc_message->configstring);

    case Q2P_SVC_CENTERPRINT:
        return q2proto_common_server_write_centerprint(io_arg, &svc_message->centerprint);

    case Q2P_SVC_FRAME:
        return q2pro_extdemo_server_write_frame(context, io_arg, &svc_message->frame);

    case Q2P_SVC_FRAME_ENTITY_DELTA:
        return q2pro_extdemo_server_write_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);

    case Q2P_SVC_LAYOUT:
        return q2proto_common_server_write_layout(io_arg, &svc_message->layout);

    default:
        break;
    }

    /* The following messages are currently not covered,
     * as they're actually sent by game code:
     *  muzzleflash
     *  muzzleflash2
     *  temp_entity
     *  inventory
     * 'layout' is needed for demo writing, so handle it here as well.
     */

    return Q2P_ERR_NOT_IMPLEMENTED;
}

static q2proto_error_t q2pro_extdemo_server_write_serverdata(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                             const q2proto_svc_serverdata_t *serverdata)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_serverdata);
    WRITE_CHECKED(server_write, io_arg, i32, serverdata->protocol);
    WRITE_CHECKED(server_write, io_arg, i32, serverdata->servercount);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->attractloop);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->gamedir);
    WRITE_CHECKED(server_write, io_arg, i16, serverdata->clientnum);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->levelname);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_server_write_spawnbaseline(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                                const q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_spawnbaseline);
    CHECKED(server_write, io_arg,
            q2proto_q2pro_server_write_entity_state_delta(context, io_arg, spawnbaseline->entnum,
                                                          &spawnbaseline->delta_state));
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_server_write_playerstate(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                              const q2proto_svc_playerstate_t *playerstate)
{
    bool has_q2pro_extensions_v2 = context->server_info->game_api == Q2PROTO_GAME_Q2PRO_EXTENDED_V2;
    bool has_morebits = context->protocol >= Q2P_PROTOCOL_Q2PRO_EXTENDED_DEMO_PLAYERFOG;
    uint32_t flags = 0;

    if (playerstate->delta_bits & Q2P_PSD_PM_TYPE)
        flags |= PS_M_TYPE;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_origin) != 0)
        flags |= PS_M_ORIGIN;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_velocity) != 0)
        flags |= PS_M_VELOCITY;
    if (playerstate->delta_bits & Q2P_PSD_PM_TIME) {
        flags |= PS_M_TIME;
        if (!has_q2pro_extensions_v2 && playerstate->pm_time > UINT8_MAX)
            return Q2P_ERR_BAD_DATA;
    }
    if (playerstate->delta_bits & Q2P_PSD_PM_FLAGS) {
        flags |= PS_M_FLAGS;
        if (!has_q2pro_extensions_v2 && playerstate->pm_flags > UINT8_MAX)
            return Q2P_ERR_BAD_DATA;
    }
    if (playerstate->delta_bits & Q2P_PSD_PM_GRAVITY)
        flags |= PS_M_GRAVITY;
    if (playerstate->delta_bits & Q2P_PSD_PM_DELTA_ANGLES)
        flags |= PS_M_DELTA_ANGLES;
    if (playerstate->delta_bits & Q2P_PSD_VIEWOFFSET)
        flags |= PS_VIEWOFFSET;
    if (playerstate->viewangles.delta_bits != 0)
        flags |= PS_VIEWANGLES;
    if (playerstate->delta_bits & Q2P_PSD_KICKANGLES)
        flags |= PS_KICKANGLES;
    if (playerstate->blend.delta_bits != 0)
        flags |= PS_BLEND;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (playerstate->damage_blend.delta_bits != 0) {
        if (has_q2pro_extensions_v2)
            flags |= PS_BLEND;
        else
            return Q2P_ERR_BAD_DATA;
    }
#endif
    if (playerstate->delta_bits & Q2P_PSD_FOV)
        flags |= PS_FOV;
    if (playerstate->delta_bits & Q2P_PSD_RDFLAGS)
        flags |= PS_RDFLAGS;
    if (playerstate->delta_bits & (Q2P_PSD_GUNINDEX | Q2P_PSD_GUNSKIN))
        flags |= PS_WEAPONINDEX;
    if ((playerstate->delta_bits & Q2P_PSD_GUNFRAME) || (playerstate->gunoffset.delta_bits != 0)
        || (playerstate->gunangles.delta_bits != 0))
        flags |= PS_WEAPONFRAME;
    if (playerstate->delta_bits & Q2P_PSD_CLIENTNUM)
        return Q2P_ERR_BAD_DATA;
    if (!has_q2pro_extensions_v2 && playerstate->statbits > UINT32_MAX)
        return Q2P_ERR_BAD_DATA;
#if Q2PROTO_PLAYER_STATE_FEATURES == Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (playerstate->fog.flags != 0 || playerstate->fog.global.color.delta_bits != 0
        || playerstate->fog.height.start_color.delta_bits != 0 || playerstate->fog.height.end_color.delta_bits != 0)
        flags |= PS_Q2PRO_PLAYERFOG;
#endif

    if (flags > UINT16_MAX) {
        if (!has_morebits)
            return Q2P_ERR_BAD_DATA;
        flags |= PS_MOREBITS;
    }

    //
    // write it
    //
    WRITE_CHECKED(server_write, io_arg, u8, svc_playerinfo);
    WRITE_CHECKED(server_write, io_arg, u16, flags);
    if (flags & PS_MOREBITS)
        WRITE_CHECKED(server_write, io_arg, u8, flags >> 16);

    if (flags & PS_M_TYPE)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_type);

    if (flags & PS_M_ORIGIN) {
        if (has_q2pro_extensions_v2) {
            for (int c = 0; c < 3; c++) {
                int32_t prev_val = q2proto_var_coords_get_int_comp(&playerstate->pm_origin.write.prev, c);
                int32_t curr_val = q2proto_var_coords_get_int_comp(&playerstate->pm_origin.write.current, c);
                WRITE_CHECKED(server_write, io_arg, q2pro_i23, curr_val, prev_val);
            }
        } else
            WRITE_CHECKED(server_write, io_arg, var_coords_short, &playerstate->pm_origin.write.current);
    }

    if (flags & PS_M_VELOCITY) {
        if (has_q2pro_extensions_v2) {
            for (int c = 0; c < 3; c++) {
                int32_t prev_val = q2proto_var_coords_get_int_comp(&playerstate->pm_velocity.write.prev, c);
                int32_t curr_val = q2proto_var_coords_get_int_comp(&playerstate->pm_velocity.write.current, c);
                WRITE_CHECKED(server_write, io_arg, q2pro_i23, curr_val, prev_val);
            }
        } else
            WRITE_CHECKED(server_write, io_arg, var_coords_short, &playerstate->pm_velocity.write.current);
    }

    if (flags & PS_M_TIME) {
        if (has_q2pro_extensions_v2)
            WRITE_CHECKED(server_write, io_arg, u16, playerstate->pm_time);
        else
            WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_time);
    }

    if (flags & PS_M_FLAGS) {
        if (has_q2pro_extensions_v2)
            WRITE_CHECKED(server_write, io_arg, u16, playerstate->pm_flags);
        else
            WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_flags);
    }

    if (flags & PS_M_GRAVITY)
        WRITE_CHECKED(server_write, io_arg, i16, playerstate->pm_gravity);

    if (flags & PS_M_DELTA_ANGLES) {
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->pm_delta_angles, 0));
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->pm_delta_angles, 1));
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->pm_delta_angles, 2));
    }

    if (flags & PS_VIEWOFFSET) {
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_offsets_get_char_comp(&playerstate->viewoffset, 0));
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_offsets_get_char_comp(&playerstate->viewoffset, 1));
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_offsets_get_char_comp(&playerstate->viewoffset, 2));
    }

    if (flags & PS_VIEWANGLES) {
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 0));
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 1));
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 2));
    }

    if (flags & PS_KICKANGLES) {
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_angles_get_char_comp(&playerstate->kick_angles, 0));
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_angles_get_char_comp(&playerstate->kick_angles, 1));
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_angles_get_char_comp(&playerstate->kick_angles, 2));
    }

    if (flags & PS_WEAPONINDEX) {
        uint16_t gun_index_and_skin = playerstate->gunindex;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        gun_index_and_skin |= (playerstate->gunskin << Q2PRO_GUNINDEX_BITS);
#endif
        WRITE_CHECKED(server_write, io_arg, u16, gun_index_and_skin);
    }

    if (flags & PS_WEAPONFRAME) {
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->gunframe);
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_offsets_get_char_comp(&playerstate->gunoffset.values, 0));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_offsets_get_char_comp(&playerstate->gunoffset.values, 1));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_offsets_get_char_comp(&playerstate->gunoffset.values, 2));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_angles_get_char_comp(&playerstate->gunangles.values, 0));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_angles_get_char_comp(&playerstate->gunangles.values, 1));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_angles_get_char_comp(&playerstate->gunangles.values, 2));
    }

    if (flags & PS_BLEND) {
        if (has_q2pro_extensions_v2) {
            const q2proto_color_delta_t *damage_blend;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
            damage_blend = &playerstate->damage_blend;
#else
            const q2proto_color_delta_t null_blend = {0};
            damage_blend = &null_blend;
#endif
            CHECKED(server_write, io_arg, server_write_q2pro_extv2_blends(io_arg, &playerstate->blend, damage_blend));
        } else {
            WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 0));
            WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 1));
            WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 2));
            WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 3));
        }
    }

#if Q2PROTO_PLAYER_STATE_FEATURES == Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (flags & PS_Q2PRO_PLAYERFOG)
        CHECKED(server_write, io_arg, q2proto_q2pro_server_write_playerfog(context, io_arg, &playerstate->fog));
#endif

    if (flags & PS_FOV)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->fov);
    if (flags & PS_RDFLAGS)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->rdflags);

    // send stats
    int numstats = 32;
    if (has_q2pro_extensions_v2) {
        numstats = 64;
        WRITE_CHECKED(server_write, io_arg, var_u64, playerstate->statbits);
    } else
        WRITE_CHECKED(server_write, io_arg, u32, playerstate->statbits);
    for (int i = 0; i < numstats; i++)
        if (playerstate->statbits & BIT_ULL(i))
            WRITE_CHECKED(server_write, io_arg, i16, playerstate->stats[i]);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                        const q2proto_svc_frame_t *frame)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_frame);
    WRITE_CHECKED(server_write, io_arg, i32, frame->serverframe);
    WRITE_CHECKED(server_write, io_arg, i32, frame->deltaframe);
    WRITE_CHECKED(server_write, io_arg, u8, frame->suppress_count);

    // write areabits
    WRITE_CHECKED(server_write, io_arg, u8, frame->areabits_len);
    void *areabits;
    CHECKED_IO(server_write, io_arg, areabits = q2protoio_write_reserve_raw(io_arg, frame->areabits_len),
               "reserve areabits");
    memcpy(areabits, frame->areabits, frame->areabits_len);

    CHECKED(server_write, io_arg, q2pro_extdemo_server_write_playerstate(context, io_arg, &frame->playerstate));

    WRITE_CHECKED(server_write, io_arg, u8, svc_packetentities);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2pro_extdemo_server_write_frame_entity_delta(
    q2proto_servercontext_t *context, uintptr_t io_arg, const q2proto_svc_frame_entity_delta_t *frame_entity_delta)
{
    if (frame_entity_delta->remove) {
        q2proto_common_server_write_entity_bits(io_arg, U_REMOVE, frame_entity_delta->newnum);
        return Q2P_ERR_SUCCESS;
    }

    if (frame_entity_delta->newnum == 0) {
        // special case: packetentities "terminator"
        WRITE_CHECKED(server_write, io_arg, u8, 0); // bits
        WRITE_CHECKED(server_write, io_arg, u8, 0); // entnum
        return Q2P_ERR_SUCCESS;
    }

    return q2proto_q2pro_server_write_entity_state_delta(context, io_arg, frame_entity_delta->newnum,
                                                         &frame_entity_delta->entity_delta);
}

#define WRITE_GAMESTATE_FUNCTION_NAME     q2pro_extdemo_server_write_gamestate
#define WRITE_GAMESTATE_BASELINE_SIZE     Q2PRO_WRITE_GAMESTATE_BASELINE_SIZE
#define WRITE_GAMESTATE_BASELINE(C, I, S) q2pro_extdemo_server_write_spawnbaseline(C, I, S)

#include "q2proto_write_gamestate.inc"

#undef WRITE_GAMESTATE_FUNCTION_NAME
#undef WRITE_GAMESTATE_BASELINE_SIZE
#undef WRITE_GAMESTATE_BASELINE
