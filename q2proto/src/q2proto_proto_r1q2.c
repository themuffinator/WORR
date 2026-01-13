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

q2proto_error_t q2proto_r1q2_parse_connect(q2proto_string_t *connect_str, q2proto_connect_t *parsed_connect)
{
    // set minor protocol version
    q2proto_string_t protocol_ver_token = {0};
    next_token(&protocol_ver_token, connect_str, ' ');
    if (protocol_ver_token.len > 0) {
        parsed_connect->version = q2pstol(&protocol_ver_token, 10);
        if (parsed_connect->version < PROTOCOL_VERSION_R1Q2_MINIMUM)
            parsed_connect->version = PROTOCOL_VERSION_R1Q2_MINIMUM;
        else if (parsed_connect->version > PROTOCOL_VERSION_R1Q2_CURRENT)
            parsed_connect->version = PROTOCOL_VERSION_R1Q2_CURRENT;
    } else {
        parsed_connect->version = PROTOCOL_VERSION_R1Q2_MINIMUM;
    }
    parsed_connect->has_zlib = true;

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_r1q2_complete_connect(q2proto_connect_t *connect)
{
    if (connect->version == 0)
        connect->version = PROTOCOL_VERSION_R1Q2_CURRENT;
    connect->has_zlib = Q2PROTO_COMPRESSION_DEFLATE;
    connect->qport &= 0xff;
    return Q2P_ERR_SUCCESS;
}

const char *q2proto_r1q2_connect_tail(const q2proto_connect_t *connect)
{
    return q2proto_va("%d %d", connect->packet_length, connect->version);
}

//
// CLIENT: PARSE MESSAGES FROM SERVER
//

static q2proto_error_t r1q2_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                        q2proto_svc_message_t *svc_message);
static q2proto_error_t r1q2_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                           q2proto_svc_frame_entity_delta_t *frame_entity_delta);
static q2proto_error_t r1q2_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                         const q2proto_clc_message_t *clc_message);
static uint32_t r1q2_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins, const q2proto_vec3_t maxs);
static void r1q2_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                              q2proto_vec3_t maxs);

q2proto_error_t q2proto_r1q2_continue_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 q2proto_svc_serverdata_t *serverdata)
{
    context->pack_solid = r1q2_pack_solid;
    context->unpack_solid = r1q2_unpack_solid;

    READ_CHECKED(client_read, io_arg, serverdata->servercount, i32);
    READ_CHECKED(client_read, io_arg, serverdata->attractloop, bool);
    READ_CHECKED(client_read, io_arg, serverdata->gamedir, string);
    READ_CHECKED(client_read, io_arg, serverdata->clientnum, i16);
    READ_CHECKED(client_read, io_arg, serverdata->levelname, string);
    READ_CHECKED(client_read, io_arg, serverdata->r1q2.enhanced, bool);
    READ_CHECKED(client_read, io_arg, serverdata->protocol_version, u16);
    CHECKED_IO(client_read, io_arg, q2protoio_read_u8(io_arg), "skip advanced deltas");
    READ_CHECKED(client_read, io_arg, serverdata->strafejump_hack, bool);

    context->client_read = r1q2_client_read;
    context->client_write = r1q2_client_write;
    context->server_protocol = Q2P_PROTOCOL_R1Q2;
    context->protocol_version = serverdata->protocol_version;

    context->features.has_upmove = true;
    context->features.has_solid32 = serverdata->protocol_version >= PROTOCOL_VERSION_R1Q2_LONG_SOLID;

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                   q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t r1q2_client_read_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg, uint64_t bits,
                                                     q2proto_entity_state_delta_t *entity_state);
static q2proto_error_t r1q2_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t r1q2_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg, uint8_t extrabits,
                                              q2proto_svc_frame_t *frame);
static q2proto_error_t r1q2_client_read_zdownload(uintptr_t io_arg, q2proto_svc_download_t *download);

static MAYBE_UNUSED const char *r1q2_server_cmd_string(int command)
{
#define S(X) \
    case X:  \
        return #X;

    switch (command) {
        S(svc_r1q2_setting)
        S(svc_r1q2_zdownload)
        S(svc_r1q2_zpacket)
    }

#undef S

    const char *str = q2proto_debug_common_svc_string(command);
    return str ? str : q2proto_va("%d", command);
}

static q2proto_error_t r1q2_client_read(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
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
            return r1q2_client_read(context, raw_io_arg, svc_message);
        }
#endif
        return Q2P_ERR_NO_MORE_INPUT;
    }

    uint8_t command = *(const uint8_t *)command_ptr;
    uint8_t extrabits = command & 0xE0;
    command &= 0x1F;
    SHOWNET(io_arg, 1, -1, "%s", r1q2_server_cmd_string(command));

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
        return r1q2_client_read_serverdata(context, io_arg, &svc_message->serverdata);

    case svc_configstring:
        svc_message->type = Q2P_SVC_CONFIGSTRING;
        return q2proto_common_client_read_configstring(io_arg, &svc_message->configstring);

    case svc_sound:
        svc_message->type = Q2P_SVC_SOUND;
        return q2proto_common_client_read_sound_short(io_arg, &svc_message->sound);

    case svc_spawnbaseline:
        svc_message->type = Q2P_SVC_SPAWNBASELINE;
        return r1q2_client_read_baseline(context, io_arg, &svc_message->spawnbaseline);

    case svc_temp_entity:
        svc_message->type = Q2P_SVC_TEMP_ENTITY;
        return q2proto_common_client_read_temp_entity_short(io_arg, context->features.server_game_api,
                                                            &svc_message->temp_entity);

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
        return r1q2_client_read_frame(context, io_arg, extrabits, &svc_message->frame);

    case svc_inventory:
        svc_message->type = Q2P_SVC_INVENTORY;
        return q2proto_common_client_read_inventory(io_arg, &svc_message->inventory);

    case svc_layout:
        svc_message->type = Q2P_SVC_LAYOUT;
        return q2proto_common_client_read_layout(io_arg, &svc_message->layout);

    case svc_r1q2_zpacket:
        CHECKED(client_read, io_arg, r1q2_client_read_zpacket(context, io_arg, svc_message));
        // Call recursively to pick up first message from zpacket
        return r1q2_client_read(context, raw_io_arg, svc_message);

    case svc_r1q2_zdownload:
        svc_message->type = Q2P_SVC_DOWNLOAD;
        return r1q2_client_read_zdownload(io_arg, &svc_message->download);

        /* There could also be "svc_playerupdate" (23), but that has to be explicitly enabled, and was default-off in
         * R1Q2 anyway, so don't bother with it */

    case svc_r1q2_setting:
        svc_message->type = Q2P_SVC_SETTING;
        return r1q2_client_read_setting(io_arg, &svc_message->setting);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t r1q2_client_read_delta_entities(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                       q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    // zpacket might contain multiple packets, so try to read from inflated message repeatedly
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    svc_message->type = Q2P_SVC_FRAME_ENTITY_DELTA;
    q2proto_error_t err = r1q2_client_next_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);
    if (err != Q2P_ERR_SUCCESS) {
        // FIXME: May be insufficient, might need some explicit way to reset parsing...
        context->client_read = r1q2_client_read;
        return err;
    }

    if (svc_message->frame_entity_delta.newnum == 0) {
        context->client_read = r1q2_client_read;
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                           q2proto_svc_frame_entity_delta_t *frame_entity_delta)
{
    memset(frame_entity_delta, 0, sizeof(*frame_entity_delta));

    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &frame_entity_delta->newnum));
    if (bits & U_MOREBITS4)
        return Q2P_ERR_BAD_DATA;

    q2proto_debug_shownet_entity_delta_bits(io_arg, "   delta:", frame_entity_delta->newnum, bits);

    if (frame_entity_delta->newnum == 0) {
        return Q2P_ERR_SUCCESS;
    }

    if (bits & U_REMOVE) {
        frame_entity_delta->remove = true;
        return Q2P_ERR_SUCCESS;
    }

    return r1q2_client_read_entity_delta(context, io_arg, bits, &frame_entity_delta->entity_delta);
}

static q2proto_error_t r1q2_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                   q2proto_svc_serverdata_t *serverdata)
{
    int32_t protocol;
    READ_CHECKED(client_read, io_arg, protocol, i32);

    if (protocol != PROTOCOL_R1Q2)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "unexpected protocol %d", protocol);

    serverdata->protocol = protocol;

    return q2proto_r1q2_continue_serverdata(context, io_arg, serverdata);
}

static q2proto_error_t r1q2_client_read_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg, uint64_t bits,
                                                     q2proto_entity_state_delta_t *entity_state)
{
    if (delta_bits_check(bits, U_MODEL, &entity_state->delta_bits, Q2P_ESD_MODELINDEX))
        READ_CHECKED(client_read, io_arg, entity_state->modelindex, u8);
    if (delta_bits_check(bits, U_MODEL2, &entity_state->delta_bits, Q2P_ESD_MODELINDEX2))
        READ_CHECKED(client_read, io_arg, entity_state->modelindex2, u8);
    if (delta_bits_check(bits, U_MODEL3, &entity_state->delta_bits, Q2P_ESD_MODELINDEX3))
        READ_CHECKED(client_read, io_arg, entity_state->modelindex3, u8);
    if (delta_bits_check(bits, U_MODEL4, &entity_state->delta_bits, Q2P_ESD_MODELINDEX4))
        READ_CHECKED(client_read, io_arg, entity_state->modelindex4, u8);

    if (delta_bits_check(bits, U_FRAME8, &entity_state->delta_bits, Q2P_ESD_FRAME))
        READ_CHECKED(client_read, io_arg, entity_state->frame, u8);
    else if (delta_bits_check(bits, U_FRAME16, &entity_state->delta_bits, Q2P_ESD_FRAME))
        READ_CHECKED(client_read, io_arg, entity_state->frame, u16);

    if (delta_bits_check(bits, U_SKIN32, &entity_state->delta_bits, Q2P_ESD_SKINNUM)) {
        if ((bits & U_SKIN32) == U_SKIN32) // used for laser colors
            READ_CHECKED(client_read, io_arg, entity_state->skinnum, u32);
        else if (bits & U_SKIN16)
            READ_CHECKED(client_read, io_arg, entity_state->skinnum, u16);
        else if (bits & U_SKIN8)
            READ_CHECKED(client_read, io_arg, entity_state->skinnum, u8);
    }

    if (delta_bits_check(bits, U_EFFECTS32, &entity_state->delta_bits, Q2P_ESD_EFFECTS)) {
        if ((bits & U_EFFECTS32) == U_EFFECTS32)
            READ_CHECKED(client_read, io_arg, entity_state->effects, u32);
        else if (bits & U_EFFECTS16)
            READ_CHECKED(client_read, io_arg, entity_state->effects, u16);
        else if (bits & U_EFFECTS8)
            READ_CHECKED(client_read, io_arg, entity_state->effects, u8);
    }

    if (delta_bits_check(bits, U_RENDERFX32, &entity_state->delta_bits, Q2P_ESD_RENDERFX)) {
        if ((bits & U_RENDERFX32) == U_RENDERFX32)
            READ_CHECKED(client_read, io_arg, entity_state->renderfx, u32);
        else if (bits & U_RENDERFX16)
            READ_CHECKED(client_read, io_arg, entity_state->renderfx, u16);
        else if (bits & U_RENDERFX8)
            READ_CHECKED(client_read, io_arg, entity_state->renderfx, u8);
    }

    entity_state->origin.read.value.delta_bits = 0;
    if (bits & U_ORIGIN1) {
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &entity_state->origin.read.value.values, 0);
        entity_state->origin.read.value.delta_bits |= BIT(0);
    }
    if (bits & U_ORIGIN2) {
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &entity_state->origin.read.value.values, 1);
        entity_state->origin.read.value.delta_bits |= BIT(1);
    }
    if (bits & U_ORIGIN3) {
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &entity_state->origin.read.value.values, 2);
        entity_state->origin.read.value.delta_bits |= BIT(2);
    }

    entity_state->angle.delta_bits = 0;
    if (bits & U_ANGLE1) {
        READ_CHECKED_VAR_ANGLES_COMP_8(client_read, io_arg, &entity_state->angle.values, 0);
        entity_state->angle.delta_bits |= BIT(0);
    }
    if (bits & U_ANGLE2) {
        READ_CHECKED_VAR_ANGLES_COMP_8(client_read, io_arg, &entity_state->angle.values, 1);
        entity_state->angle.delta_bits |= BIT(1);
    }
    if (bits & U_ANGLE3) {
        READ_CHECKED_VAR_ANGLES_COMP_8(client_read, io_arg, &entity_state->angle.values, 2);
        entity_state->angle.delta_bits |= BIT(2);
    }

    if (delta_bits_check(bits, U_OLDORIGIN, &entity_state->delta_bits, Q2P_ESD_OLD_ORIGIN))
        CHECKED(client_read, io_arg, read_var_coords_short(io_arg, &entity_state->old_origin));

    if (delta_bits_check(bits, U_SOUND, &entity_state->delta_bits, Q2P_ESD_SOUND))
        READ_CHECKED(client_read, io_arg, entity_state->sound, u8);

    if (delta_bits_check(bits, U_EVENT, &entity_state->delta_bits, Q2P_ESD_EVENT))
        READ_CHECKED(client_read, io_arg, entity_state->event, u8);

    if (delta_bits_check(bits, U_SOLID, &entity_state->delta_bits, Q2P_ESD_SOLID)) {
        if (context->protocol_version >= PROTOCOL_VERSION_R1Q2_LONG_SOLID)
            READ_CHECKED(client_read, io_arg, entity_state->solid, u32);
        else
            READ_CHECKED(client_read, io_arg, entity_state->solid, u16);
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &spawnbaseline->entnum));
    if (bits & U_MOREBITS4)
        return Q2P_ERR_BAD_DATA;

    q2proto_debug_shownet_entity_delta_bits(io_arg, "   baseline:", spawnbaseline->entnum, bits);

    return r1q2_client_read_entity_delta(context, io_arg, bits, &spawnbaseline->delta_state);
}

static q2proto_error_t r1q2_client_read_playerstate(uintptr_t io_arg, uint8_t extraflags,
                                                    q2proto_svc_playerstate_t *playerstate)
{
    uint16_t flags;
    READ_CHECKED(client_read, io_arg, flags, u16);

#if Q2PROTO_SHOWNET
    if (q2protodbg_shownet_check(io_arg, 2) && flags) {
        char buf[1024], buf2[1024];
        q2proto_debug_common_player_delta_bits_to_str(buf, sizeof(buf), flags);
        q2proto_debug_common_player_delta_extrabits_to_str(buf2, sizeof(buf2), extraflags);
        SHOWNET(io_arg, 2, -2, "   %s + %s", buf, buf2);
    }
#endif

    //
    // parse the pmove_state_t
    //
    if (delta_bits_check(flags, PS_M_TYPE, &playerstate->delta_bits, Q2P_PSD_PM_TYPE))
        READ_CHECKED(client_read, io_arg, playerstate->pm_type, u8);

    playerstate->pm_origin.read.value.delta_bits = 0;
    if (flags & PS_M_ORIGIN) {
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &playerstate->pm_origin.read.value.values, 0);
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &playerstate->pm_origin.read.value.values, 1);
        playerstate->pm_origin.read.value.delta_bits |= BIT(0) | BIT(1);
    }
    if (extraflags & EPS_M_ORIGIN2) {
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &playerstate->pm_origin.read.value.values, 2);
        playerstate->pm_origin.read.value.delta_bits |= BIT(2);
    }

    playerstate->pm_velocity.read.value.delta_bits = 0;
    if (flags & PS_M_VELOCITY) {
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &playerstate->pm_velocity.read.value.values, 0);
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &playerstate->pm_velocity.read.value.values, 1);
        playerstate->pm_velocity.read.value.delta_bits |= BIT(0) | BIT(1);
    }
    if (extraflags & EPS_M_VELOCITY2) {
        READ_CHECKED_VAR_COORDS_COMP_16(client_read, io_arg, &playerstate->pm_velocity.read.value.values, 2);
        playerstate->pm_velocity.read.value.delta_bits |= BIT(2);
    }

    if (delta_bits_check(flags, PS_M_TIME, &playerstate->delta_bits, Q2P_PSD_PM_TIME))
        READ_CHECKED(client_read, io_arg, playerstate->pm_time, u8);

    if (delta_bits_check(flags, PS_M_FLAGS, &playerstate->delta_bits, Q2P_PSD_PM_FLAGS))
        READ_CHECKED(client_read, io_arg, playerstate->pm_flags, u8);

    if (delta_bits_check(flags, PS_M_GRAVITY, &playerstate->delta_bits, Q2P_PSD_PM_GRAVITY))
        READ_CHECKED(client_read, io_arg, playerstate->pm_gravity, i16);

    if (delta_bits_check(flags, PS_M_DELTA_ANGLES, &playerstate->delta_bits, Q2P_PSD_PM_DELTA_ANGLES))
        CHECKED(client_read, io_arg, read_var_angles16(io_arg, &playerstate->pm_delta_angles));

    //
    // parse the rest of the player_state_t
    //
    if (delta_bits_check(flags, PS_VIEWOFFSET, &playerstate->delta_bits, Q2P_PSD_VIEWOFFSET))
        CHECKED(client_read, io_arg, read_var_small_offsets(io_arg, &playerstate->viewoffset));

    playerstate->viewangles.delta_bits = 0;
    if (flags & PS_VIEWANGLES) {
        READ_CHECKED_VAR_ANGLES_COMP_16(client_read, io_arg, &playerstate->viewangles.values, 0);
        READ_CHECKED_VAR_ANGLES_COMP_16(client_read, io_arg, &playerstate->viewangles.values, 1);
        playerstate->viewangles.delta_bits |= BIT(0) | BIT(1);
    }
    if (extraflags & EPS_VIEWANGLE2) {
        READ_CHECKED_VAR_ANGLES_COMP_16(client_read, io_arg, &playerstate->viewangles.values, 2);
        playerstate->viewangles.delta_bits |= BIT(2);
    }

    if (delta_bits_check(flags, PS_KICKANGLES, &playerstate->delta_bits, Q2P_PSD_KICKANGLES))
        CHECKED(client_read, io_arg, read_var_small_angles(io_arg, &playerstate->kick_angles));

    if (delta_bits_check(flags, PS_WEAPONINDEX, &playerstate->delta_bits, Q2P_PSD_GUNINDEX))
        READ_CHECKED(client_read, io_arg, playerstate->gunindex, u8);

    if (delta_bits_check(flags, PS_WEAPONFRAME, &playerstate->delta_bits, Q2P_PSD_GUNFRAME))
        READ_CHECKED(client_read, io_arg, playerstate->gunframe, u8);
    if (extraflags & EPS_GUNOFFSET) {
        CHECKED(client_read, io_arg, read_var_small_offsets(io_arg, &playerstate->gunoffset.values));
        playerstate->gunoffset.delta_bits = BIT(0) | BIT(1) | BIT(2);
    }
    if (extraflags & EPS_GUNANGLES) {
        CHECKED(client_read, io_arg, read_var_small_angles(io_arg, &playerstate->gunangles.values));
        playerstate->gunangles.delta_bits = BIT(0) | BIT(1) | BIT(2);
    }

    if (flags & PS_BLEND) {
        CHECKED(client_read, io_arg, read_var_color(io_arg, &playerstate->blend.values));
        playerstate->blend.delta_bits = 0xf;
    }

    if (delta_bits_check(flags, PS_FOV, &playerstate->delta_bits, Q2P_PSD_FOV))
        READ_CHECKED(client_read, io_arg, playerstate->fov, u8);

    if (delta_bits_check(flags, PS_RDFLAGS, &playerstate->delta_bits, Q2P_PSD_RDFLAGS))
        READ_CHECKED(client_read, io_arg, playerstate->rdflags, u8);

    // parse stats
    if (extraflags & EPS_STATS) {
        READ_CHECKED(client_read, io_arg, playerstate->statbits, u32);
        for (int i = 0; i < 32; i++)
            if (playerstate->statbits & (1 << i))
                READ_CHECKED(client_read, io_arg, playerstate->stats[i], i16);
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg, uint8_t extrabits,
                                              q2proto_svc_frame_t *frame)
{
    int32_t serverframe;
    READ_CHECKED(client_read, io_arg, serverframe, i32);

    uint32_t offset = serverframe & 0xF8000000;
    offset >>= 27;

    serverframe &= 0x07FFFFFF;

    frame->serverframe = serverframe;

    if (offset == 31)
        frame->deltaframe = -1;
    else
        frame->deltaframe = serverframe - offset;

    uint8_t extraflags = extrabits >> 1;
    READ_CHECKED(client_read, io_arg, frame->suppress_count, u8);
    extraflags |= (frame->suppress_count & 0xF0) >> 4;
    frame->suppress_count &= 0x0F;

    // read areabits
    READ_CHECKED(client_read, io_arg, frame->areabits_len, u8);
    READ_CHECKED(client_read, io_arg, frame->areabits, raw, frame->areabits_len, NULL);

    // read playerinfo
    SHOWNET(io_arg, 2, 0, "playerinfo");
    CHECKED(client_read, io_arg, r1q2_client_read_playerstate(io_arg, extraflags, &frame->playerstate));

    // read packet entities
    context->client_read = r1q2_client_read_delta_entities;
    SHOWNET(io_arg, 2, 0, "packetentities");

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t r1q2_client_read_zpacket(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                         q2proto_svc_message_t *svc_message)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    uint16_t compressed_len, uncompressed_len;
    READ_CHECKED(client_read, io_arg, compressed_len, u16);
    READ_CHECKED(client_read, io_arg, uncompressed_len, u16);
    (void)uncompressed_len;

    uintptr_t inflate_io_arg;
    CHECKED(client_read, io_arg, q2protoio_inflate_begin(io_arg, Q2P_INFL_DEFL_RAW, &inflate_io_arg));
    CHECKED(client_read, io_arg, q2protoio_inflate_data(io_arg, inflate_io_arg, compressed_len));
    context->has_inflate_io_arg = true;
    context->inflate_io_arg = inflate_io_arg;
    return Q2P_ERR_SUCCESS;
#else
    return Q2P_ERR_DEFLATE_NOT_SUPPORTED;
#endif
}

static q2proto_error_t r1q2_client_read_zdownload(uintptr_t io_arg, q2proto_svc_download_t *download)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    READ_CHECKED(client_read, io_arg, download->size, i16);
    READ_CHECKED(client_read, io_arg, download->percent, u8);

    uint16_t uncompressed_len;
    READ_CHECKED(client_read, io_arg, uncompressed_len, u16);

    uintptr_t inflate_io_arg;
    CHECKED(client_read, io_arg, q2protoio_inflate_begin(io_arg, Q2P_INFL_DEFL_RAW, &inflate_io_arg));
    CHECKED(client_read, io_arg, q2protoio_inflate_data(io_arg, inflate_io_arg, download->size));
    READ_CHECKED(client_read, inflate_io_arg, download->data, raw, uncompressed_len, NULL);
    CHECKED_IO(client_read, io_arg, q2protoio_inflate_end(inflate_io_arg), "finishing inflate");
    download->size = uncompressed_len;
    return Q2P_ERR_SUCCESS;
#else
    return Q2P_ERR_DEFLATE_NOT_SUPPORTED;
#endif
}

q2proto_error_t r1q2_client_read_setting(uintptr_t io_arg, q2proto_svc_setting_t *setting)
{
    READ_CHECKED(client_read, io_arg, setting->index, i32);
    READ_CHECKED(client_read, io_arg, setting->value, i32);
    return Q2P_ERR_SUCCESS;
}

//
// CLIENT: SEND MESSAGES TO SERVER
//

static q2proto_error_t r1q2_client_write_move(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                              const q2proto_clc_move_t *move);

static q2proto_error_t r1q2_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                         const q2proto_clc_message_t *clc_message)
{
    switch (clc_message->type) {
    case Q2P_CLC_NOP:
        return q2proto_common_client_write_nop(io_arg);

    case Q2P_CLC_MOVE:
        return r1q2_client_write_move(context, io_arg, &clc_message->move);

    case Q2P_CLC_USERINFO:
        return q2proto_common_client_write_userinfo(io_arg, &clc_message->userinfo);

    case Q2P_CLC_STRINGCMD:
        return q2proto_common_client_write_stringcmd(io_arg, &clc_message->stringcmd);

    case Q2P_CLC_SETTING:
        return r1q2_client_write_setting(io_arg, &clc_message->setting);

    default:
        break;
    }

    return Q2P_ERR_BAD_COMMAND;
}

// stolen for r1q2 in the name of bandwidth
// If set, movement values are sent "compressed" via division by 5.
#define BUTTON_UCMD_DBLFORWARD BIT(2)
#define BUTTON_UCMD_DBLSIDE    BIT(3)
#define BUTTON_UCMD_DBLUP      BIT(4)

// If set, angles[0] is sent divided by 64
#define BUTTON_UCMD_DBL_ANGLE1 BIT(5)
// If set, angles[1] is sent divided by 256
#define BUTTON_UCMD_DBL_ANGLE2 BIT(6)

static q2proto_error_t r1q2_client_write_move_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                    const q2proto_clc_move_delta_t *move_delta)
{
    uint8_t bits = 0;
    uint8_t buttons = 0;
    if (move_delta->delta_bits & Q2P_CMD_ANGLE0)
        bits |= CM_ANGLE1;
    if (move_delta->delta_bits & Q2P_CMD_ANGLE1)
        bits |= CM_ANGLE2;
    if (move_delta->delta_bits & Q2P_CMD_ANGLE2)
        bits |= CM_ANGLE3;
    if (move_delta->delta_bits & Q2P_CMD_MOVE_FORWARD)
        bits |= CM_FORWARD;
    if (move_delta->delta_bits & Q2P_CMD_MOVE_SIDE)
        bits |= CM_SIDE;
    if (move_delta->delta_bits & Q2P_CMD_MOVE_UP)
        bits |= CM_UP;
    if (move_delta->delta_bits & Q2P_CMD_BUTTONS) {
        bits |= CM_BUTTONS;
        buttons = move_delta->buttons;
    }
    if (move_delta->delta_bits & Q2P_CMD_IMPULSE)
        bits |= CM_IMPULSE;

    WRITE_CHECKED(client_write, io_arg, u8, bits);

    int16_t short_move[3];
    int16_t short_angles[3];
    q2proto_var_coords_get_short_unscaled(&move_delta->move, short_move);
    q2proto_var_angles_get_short(&move_delta->angles, short_angles);

    bool compressed_movements = context->protocol_version >= PROTOCOL_VERSION_R1Q2_UCMD;

    if (compressed_movements && bits & CM_BUTTONS) {
        if ((bits & CM_FORWARD) && (short_move[0] % 5) == 0)
            buttons |= BUTTON_UCMD_DBLFORWARD;
        if ((bits & CM_SIDE) && (short_move[1] % 5) == 0)
            buttons |= BUTTON_UCMD_DBLSIDE;
        if ((bits & CM_UP) && (short_move[2] % 5) == 0)
            buttons |= BUTTON_UCMD_DBLUP;

        if ((bits & CM_ANGLE1) && (short_angles[0] % 64) == 0 && (abs(short_angles[0] / 64)) < 128)
            buttons |= BUTTON_UCMD_DBL_ANGLE1;
        if ((bits & CM_ANGLE2) && (short_angles[1] % 256) == 0)
            buttons |= BUTTON_UCMD_DBL_ANGLE2;

        WRITE_CHECKED(client_write, io_arg, u8, buttons);
    }

    if (bits & CM_ANGLE1) {
        if (buttons & BUTTON_UCMD_DBL_ANGLE1)
            WRITE_CHECKED(client_write, io_arg, i8, short_angles[0] / 64);
        else
            WRITE_CHECKED(client_write, io_arg, i16, short_angles[0]);
    }
    if (bits & CM_ANGLE2) {
        if (buttons & BUTTON_UCMD_DBL_ANGLE2)
            WRITE_CHECKED(client_write, io_arg, i8, short_angles[1] / 256);
        else
            WRITE_CHECKED(client_write, io_arg, i16, short_angles[1]);
    }
    if (bits & CM_ANGLE3)
        WRITE_CHECKED(client_write, io_arg, i16, short_angles[2]);

    if (bits & CM_FORWARD) {
        if (buttons & BUTTON_UCMD_DBLFORWARD)
            WRITE_CHECKED(client_write, io_arg, i8, short_move[0] / 5);
        else
            WRITE_CHECKED(client_write, io_arg, i16, short_move[0]);
    }
    if (bits & CM_SIDE) {
        if (buttons & BUTTON_UCMD_DBLSIDE)
            WRITE_CHECKED(client_write, io_arg, i8, short_move[1] / 5);
        else
            WRITE_CHECKED(client_write, io_arg, i16, short_move[1]);
    }
    if (bits & CM_UP) {
        if (buttons & BUTTON_UCMD_DBLUP)
            WRITE_CHECKED(client_write, io_arg, i8, short_move[2] / 5);
        else
            WRITE_CHECKED(client_write, io_arg, i16, short_move[2]);
    }

    if (!compressed_movements && bits & CM_BUTTONS)
        WRITE_CHECKED(client_write, io_arg, u8, move_delta->buttons);
    if (bits & CM_IMPULSE)
        WRITE_CHECKED(client_write, io_arg, u8, move_delta->impulse);

    WRITE_CHECKED(client_write, io_arg, u8, move_delta->msec);
    WRITE_CHECKED(client_write, io_arg, u8, move_delta->lightlevel);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_client_write_move(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                              const q2proto_clc_move_t *move)
{
    WRITE_CHECKED(client_write, io_arg, u8, clc_move);
    WRITE_CHECKED(client_write, io_arg, i32, move->lastframe);
    for (int i = 0; i < 3; i++) {
        CHECKED(client_write, io_arg, r1q2_client_write_move_delta(context, io_arg, &move->moves[i]));
    }
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t r1q2_client_write_setting(uintptr_t io_arg, const q2proto_clc_setting_t *setting)
{
    WRITE_CHECKED(client_write, io_arg, u8, clc_r1q2_setting);
    WRITE_CHECKED(client_write, io_arg, i16, setting->index);
    WRITE_CHECKED(client_write, io_arg, i16, setting->value);
    return Q2P_ERR_SUCCESS;
}

static uint32_t r1q2_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins, const q2proto_vec3_t maxs)
{
    return context->protocol_version >= PROTOCOL_VERSION_R1Q2_LONG_SOLID ? q2proto_pack_solid_32_r1q2(mins, maxs)
                                                                         : q2proto_pack_solid_16(mins, maxs);
}

static void r1q2_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                              q2proto_vec3_t maxs)
{
    if (context->protocol_version >= PROTOCOL_VERSION_R1Q2_LONG_SOLID)
        q2proto_unpack_solid_32_r1q2(solid, mins, maxs);
    else
        q2proto_unpack_solid_16(solid, mins, maxs);
}

//
// SERVER: INITIALIZATION
//

static q2proto_error_t r1q2_server_fill_serverdata(q2proto_servercontext_t *context,
                                                   q2proto_svc_serverdata_t *serverdata);
static void r1q2_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                const q2proto_packed_entity_state_t *from,
                                                const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                q2proto_entity_state_delta_t *delta);
static void r1q2_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                const q2proto_packed_player_state_t *from,
                                                const q2proto_packed_player_state_t *to,
                                                q2proto_svc_playerstate_t *delta);
static q2proto_error_t r1q2_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                         const q2proto_svc_message_t *svc_message);
static q2proto_error_t r1q2_server_write_gamestate(q2proto_servercontext_t *context,
                                                   q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                   const q2proto_gamestate_t *gamestate);
static q2proto_error_t r1q2_server_read(q2proto_servercontext_t *context, uintptr_t io_arg,
                                        q2proto_clc_message_t *clc_message);

static q2proto_error_t r1q2_download_begin(q2proto_servercontext_t *context, q2proto_server_download_state_t *state,
                                           q2proto_download_compress_t compress,
                                           q2protoio_deflate_args_t *deflate_args);
static q2proto_error_t r1q2_download_data(q2proto_server_download_state_t *state, const uint8_t **data,
                                          size_t *remaining, size_t packet_remaining,
                                          q2proto_svc_download_t *svc_download);

static const struct q2proto_download_funcs_s r1q2_download_funcs = {.begin = r1q2_download_begin,
                                                                    .data = r1q2_download_data,
                                                                    .finish = q2proto_download_common_finish,
                                                                    .abort = q2proto_download_common_abort};

q2proto_error_t q2proto_r1q2_init_servercontext(q2proto_servercontext_t *context, const q2proto_connect_t *connect_info)
{
    if (context->server_info->game_api != Q2PROTO_GAME_VANILLA)
        return Q2P_ERR_GAMETYPE_UNSUPPORTED;

    context->protocol_version = connect_info->version;
    context->zpacket_cmd = svc_r1q2_zpacket;
    context->features.enable_deflate = connect_info->has_zlib;
    context->features.has_beam_old_origin_fix = true;

    context->fill_serverdata = r1q2_server_fill_serverdata;
    context->make_entity_state_delta = r1q2_server_make_entity_state_delta;
    context->make_player_state_delta = r1q2_server_make_player_state_delta;
    context->server_write = r1q2_server_write;
    context->server_write_gamestate = r1q2_server_write_gamestate;
    context->server_read = r1q2_server_read;
    context->download_funcs = &r1q2_download_funcs;
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_fill_serverdata(q2proto_servercontext_t *context,
                                                   q2proto_svc_serverdata_t *serverdata)
{
    serverdata->protocol = PROTOCOL_R1Q2;
    serverdata->protocol_version = context->protocol_version;
    return Q2P_ERR_SUCCESS;
}

static void r1q2_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                const q2proto_packed_entity_state_t *from,
                                                const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                q2proto_entity_state_delta_t *delta)
{
    q2proto_packing_make_entity_state_delta(from, to, write_old_origin, false, delta);
}

static void r1q2_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                const q2proto_packed_player_state_t *from,
                                                const q2proto_packed_player_state_t *to,
                                                q2proto_svc_playerstate_t *delta)
{
    q2proto_packing_make_player_state_delta(from, to, delta);
}

static q2proto_error_t r1q2_server_write_serverdata(uintptr_t io_arg, const q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t r1q2_server_write_spawnbaseline(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                       const q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t r1q2_server_write_download(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                  const q2proto_svc_download_t *download);
static q2proto_error_t r1q2_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                               const q2proto_svc_frame_t *frame);
static q2proto_error_t r1q2_server_write_frame_entity_delta(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                            const q2proto_svc_frame_entity_delta_t *frame_entity_delta);

static q2proto_error_t r1q2_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
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
        return q2proto_common_server_write_sound(Q2P_PROTOCOL_MULTICAST_SHORT, io_arg, &svc_message->sound);

    case Q2P_SVC_PRINT:
        return q2proto_common_server_write_print(io_arg, &svc_message->print);

    case Q2P_SVC_STUFFTEXT:
        return q2proto_common_server_write_stufftext(io_arg, &svc_message->stufftext);

    case Q2P_SVC_SERVERDATA:
        return r1q2_server_write_serverdata(io_arg, &svc_message->serverdata);

    case Q2P_SVC_CONFIGSTRING:
        return q2proto_common_server_write_configstring(io_arg, &svc_message->configstring);

    case Q2P_SVC_SPAWNBASELINE:
        return r1q2_server_write_spawnbaseline(context, io_arg, &svc_message->spawnbaseline);

    case Q2P_SVC_CENTERPRINT:
        return q2proto_common_server_write_centerprint(io_arg, &svc_message->centerprint);

    case Q2P_SVC_DOWNLOAD:
        return r1q2_server_write_download(context, io_arg, &svc_message->download);

    case Q2P_SVC_FRAME:
        return r1q2_server_write_frame(context, io_arg, &svc_message->frame);

    case Q2P_SVC_FRAME_ENTITY_DELTA:
        return r1q2_server_write_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);

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

static q2proto_error_t r1q2_server_write_serverdata(uintptr_t io_arg, const q2proto_svc_serverdata_t *serverdata)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_serverdata);
    WRITE_CHECKED(server_write, io_arg, i32, PROTOCOL_R1Q2);
    WRITE_CHECKED(server_write, io_arg, i32, serverdata->servercount);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->attractloop);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->gamedir);
    WRITE_CHECKED(server_write, io_arg, i16, serverdata->clientnum);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->levelname);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->r1q2.enhanced);
    WRITE_CHECKED(server_write, io_arg, u16, serverdata->protocol_version);
    WRITE_CHECKED(server_write, io_arg, u8, 0); // advanced delta
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->strafejump_hack);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_write_entity_state_delta(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                            uint16_t entnum,
                                                            const q2proto_entity_state_delta_t *entity_state_delta)
{
    uint32_t bits = 0;

    unsigned origin_changes = q2proto_maybe_diff_coords_write_differs_int(&entity_state_delta->origin);
    if (origin_changes & BIT(0))
        bits |= U_ORIGIN1;
    if (origin_changes & BIT(1))
        bits |= U_ORIGIN2;
    if (origin_changes & BIT(2))
        bits |= U_ORIGIN3;

    if (entity_state_delta->angle.delta_bits & BIT(0))
        bits |= U_ANGLE1;
    if (entity_state_delta->angle.delta_bits & BIT(1))
        bits |= U_ANGLE2;
    if (entity_state_delta->angle.delta_bits & BIT(2))
        bits |= U_ANGLE3;

    if (entity_state_delta->delta_bits & Q2P_ESD_SKINNUM)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->skinnum, U_SKIN8, U_SKIN16, false);

    if (entity_state_delta->delta_bits & Q2P_ESD_FRAME) {
        if (entity_state_delta->frame >= 256)
            bits |= U_FRAME16;
        else
            bits |= U_FRAME8;
    }

    if (entity_state_delta->delta_bits & Q2P_ESD_EFFECTS)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->effects, U_EFFECTS8, U_EFFECTS16, false);
    if (entity_state_delta->delta_bits & Q2P_ESD_EFFECTS_MORE)
        return Q2P_ERR_BAD_DATA;

    if (entity_state_delta->delta_bits & Q2P_ESD_RENDERFX)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->renderfx, U_RENDERFX8, U_RENDERFX16, false);

    if (entity_state_delta->delta_bits & Q2P_ESD_SOLID)
        bits |= U_SOLID;

    if (entity_state_delta->delta_bits & Q2P_ESD_EVENT)
        bits |= U_EVENT;

    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX)
        bits |= U_MODEL;
    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX2)
        bits |= U_MODEL2;
    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX3)
        bits |= U_MODEL3;
    if (entity_state_delta->delta_bits & Q2P_ESD_MODELINDEX4)
        bits |= U_MODEL4;

    if (entity_state_delta->delta_bits & Q2P_ESD_SOUND)
        bits |= U_SOUND;

    if (entity_state_delta->delta_bits & (Q2P_ESD_LOOP_ATTENUATION | Q2P_ESD_LOOP_VOLUME))
        return Q2P_ERR_BAD_DATA;

    if (entity_state_delta->delta_bits & Q2P_ESD_OLD_ORIGIN)
        bits |= U_OLDORIGIN;

    if (entity_state_delta->delta_bits & (Q2P_ESD_ALPHA | Q2P_ESD_SCALE))
        return Q2P_ERR_BAD_DATA;

    //----------

    q2proto_common_server_write_entity_bits(io_arg, bits, entnum);

    if (bits & U_MODEL) {
        if (entity_state_delta->modelindex > 255)
            return Q2P_ERR_BAD_DATA;
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex);
    }
    if (bits & U_MODEL2) {
        if (entity_state_delta->modelindex2 > 255)
            return Q2P_ERR_BAD_DATA;
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex2);
    }
    if (bits & U_MODEL3) {
        if (entity_state_delta->modelindex3 > 255)
            return Q2P_ERR_BAD_DATA;
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex3);
    }
    if (bits & U_MODEL4) {
        if (entity_state_delta->modelindex4 > 255)
            return Q2P_ERR_BAD_DATA;
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex4);
    }

    if (bits & U_FRAME16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->frame);
    else if (bits & U_FRAME8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->frame);

    if ((bits & U_SKIN32) == U_SKIN32)
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->skinnum);
    else if (bits & U_SKIN16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->skinnum);
    else if (bits & U_SKIN8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->skinnum);

    if ((bits & U_EFFECTS32) == U_EFFECTS32)
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->effects);
    else if (bits & U_EFFECTS16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->effects);
    else if (bits & U_EFFECTS8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->effects);

    if ((bits & U_RENDERFX32) == U_RENDERFX32)
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->renderfx);
    else if (bits & U_RENDERFX16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->renderfx);
    else if (bits & U_RENDERFX8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->renderfx);

    if (bits & U_ORIGIN1)
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&entity_state_delta->origin.write.current, 0));
    if (bits & U_ORIGIN2)
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&entity_state_delta->origin.write.current, 1));
    if (bits & U_ORIGIN3)
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&entity_state_delta->origin.write.current, 2));

    if (bits & U_ANGLE1)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_angles_get_char_comp(&entity_state_delta->angle.values, 0));
    if (bits & U_ANGLE2)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_angles_get_char_comp(&entity_state_delta->angle.values, 1));
    if (bits & U_ANGLE3)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_angles_get_char_comp(&entity_state_delta->angle.values, 2));

    if (bits & U_OLDORIGIN) {
        WRITE_CHECKED(server_write, io_arg, u16, q2proto_var_coords_get_int_comp(&entity_state_delta->old_origin, 0));
        WRITE_CHECKED(server_write, io_arg, u16, q2proto_var_coords_get_int_comp(&entity_state_delta->old_origin, 1));
        WRITE_CHECKED(server_write, io_arg, u16, q2proto_var_coords_get_int_comp(&entity_state_delta->old_origin, 2));
    }

    if (bits & U_SOUND) {
        if (entity_state_delta->sound > 255)
            return Q2P_ERR_BAD_DATA;
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->sound);
        // ignore loop_volume, loop_attenuation
    }
    if (bits & U_EVENT)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->event);
    if (bits & U_SOLID) {
        if (context->protocol_version >= PROTOCOL_VERSION_R1Q2_LONG_SOLID)
            WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->solid);
        else
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->solid);
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_write_spawnbaseline(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                       const q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_spawnbaseline);
    CHECKED(server_write, io_arg,
            r1q2_server_write_entity_state_delta(context, io_arg, spawnbaseline->entnum, &spawnbaseline->delta_state));
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_write_download(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                  const q2proto_svc_download_t *download)
{
    WRITE_CHECKED(server_write, io_arg, u8, download->compressed ? svc_r1q2_zdownload : svc_download);
    WRITE_CHECKED(server_write, io_arg, i16, download->size);
    WRITE_CHECKED(server_write, io_arg, u8, download->percent);
    if (download->compressed)
        WRITE_CHECKED(server_write, io_arg, i16, download->uncompressed_size);
    if (download->size > 0) {
        void *p;
        CHECKED_IO(server_write, io_arg, p = q2protoio_write_reserve_raw(io_arg, download->size),
                   "reserve download data");
        memcpy(p, download->data, download->size);
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_write_playerstate(uintptr_t io_arg, const q2proto_svc_playerstate_t *playerstate,
                                                     uint8_t *extraflags)
{
    uint16_t flags = 0;
    *extraflags = 0;

    if (playerstate->delta_bits & Q2P_PSD_PM_TYPE)
        flags |= PS_M_TYPE;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_origin) & (BIT(0) | BIT(1)))
        flags |= PS_M_ORIGIN;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_origin) & (BIT(2)))
        *extraflags |= EPS_M_ORIGIN2;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_velocity) & (BIT(0) | BIT(1)))
        flags |= PS_M_VELOCITY;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_origin) & (BIT(2)))
        *extraflags |= EPS_M_VELOCITY2;
    if (playerstate->delta_bits & Q2P_PSD_PM_TIME) {
        flags |= PS_M_TIME;
        if (playerstate->pm_time > UINT8_MAX)
            return Q2P_ERR_BAD_DATA;
    }
    if (playerstate->delta_bits & Q2P_PSD_PM_FLAGS) {
        flags |= PS_M_FLAGS;
        if (playerstate->pm_flags > UINT8_MAX)
            return Q2P_ERR_BAD_DATA;
    }
    if (playerstate->delta_bits & Q2P_PSD_PM_GRAVITY)
        flags |= PS_M_GRAVITY;
    if (playerstate->delta_bits & Q2P_PSD_PM_DELTA_ANGLES)
        flags |= PS_M_DELTA_ANGLES;
    if (playerstate->delta_bits & Q2P_PSD_PM_VIEWHEIGHT)
        return Q2P_ERR_BAD_DATA;
    if (playerstate->delta_bits & Q2P_PSD_VIEWOFFSET)
        flags |= PS_VIEWOFFSET;
    if (playerstate->viewangles.delta_bits & (BIT(0) | BIT(1)))
        flags |= PS_VIEWANGLES;
    if (playerstate->viewangles.delta_bits & (BIT(2)))
        *extraflags |= EPS_VIEWANGLE2;
    if (playerstate->delta_bits & Q2P_PSD_KICKANGLES)
        flags |= PS_KICKANGLES;
    if (playerstate->blend.delta_bits != 0)
        flags |= PS_BLEND;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (playerstate->damage_blend.delta_bits != 0)
        return Q2P_ERR_BAD_DATA;
#endif
    if (playerstate->delta_bits & Q2P_PSD_FOV)
        flags |= PS_FOV;
    if (playerstate->delta_bits & Q2P_PSD_RDFLAGS)
        flags |= PS_RDFLAGS;
    if (playerstate->delta_bits & Q2P_PSD_GUNINDEX)
        flags |= PS_WEAPONINDEX;
    if (playerstate->delta_bits & Q2P_PSD_GUNSKIN)
        return Q2P_ERR_BAD_DATA;
    if (playerstate->delta_bits & Q2P_PSD_GUNFRAME) {
        flags |= PS_WEAPONFRAME;
        if (playerstate->gunframe > UINT8_MAX)
            return Q2P_ERR_BAD_DATA;
    }
    if (playerstate->gunoffset.delta_bits != 0)
        *extraflags |= EPS_GUNOFFSET;
    if (playerstate->gunangles.delta_bits != 0)
        *extraflags |= EPS_GUNANGLES;
    if (playerstate->statbits != 0)
        *extraflags |= EPS_STATS;
    if (playerstate->delta_bits & Q2P_PSD_CLIENTNUM)
        return Q2P_ERR_BAD_DATA;
    if (playerstate->statbits > UINT32_MAX)
        return Q2P_ERR_BAD_DATA;
    if (playerstate->delta_bits & Q2P_PSD_GUNRATE)
        return Q2P_ERR_BAD_DATA;
#if Q2PROTO_PLAYER_STATE_FEATURES == Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
    if (playerstate->fog.flags != 0 || playerstate->fog.global.color.delta_bits != 0
        || playerstate->fog.height.start_color.delta_bits != 0 || playerstate->fog.height.end_color.delta_bits != 0)
        return Q2P_ERR_BAD_DATA;
#endif

    //
    // write it
    //
    WRITE_CHECKED(server_write, io_arg, u16, flags);

    if (flags & PS_M_TYPE)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_type);

    if (flags & PS_M_ORIGIN) {
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&playerstate->pm_origin.write.current, 0));
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&playerstate->pm_origin.write.current, 1));
    }
    if (*extraflags & EPS_M_ORIGIN2)
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&playerstate->pm_origin.write.current, 2));

    if (flags & PS_M_VELOCITY) {
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&playerstate->pm_velocity.write.current, 0));
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&playerstate->pm_velocity.write.current, 1));
    }
    if (*extraflags & EPS_M_VELOCITY2)
        WRITE_CHECKED(server_write, io_arg, u16,
                      q2proto_var_coords_get_int_comp(&playerstate->pm_velocity.write.current, 2));

    if (flags & PS_M_TIME)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_time);

    if (flags & PS_M_FLAGS)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_flags);

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
    }
    if (*extraflags & EPS_VIEWANGLE2)
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 2));

    if (flags & PS_KICKANGLES) {
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_angles_get_char_comp(&playerstate->kick_angles, 0));
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_angles_get_char_comp(&playerstate->kick_angles, 1));
        WRITE_CHECKED(server_write, io_arg, i8, q2proto_var_small_angles_get_char_comp(&playerstate->kick_angles, 2));
    }

    if (flags & PS_WEAPONINDEX) {
        if (playerstate->gunindex > 255)
            return Q2P_ERR_BAD_DATA;
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->gunindex);
    }

    if (flags & PS_WEAPONFRAME)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->gunframe);
    if (*extraflags & EPS_GUNOFFSET) {
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_offsets_get_char_comp(&playerstate->gunoffset.values, 0));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_offsets_get_char_comp(&playerstate->gunoffset.values, 1));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_offsets_get_char_comp(&playerstate->gunoffset.values, 2));
    }
    if (*extraflags & EPS_GUNANGLES) {
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_angles_get_char_comp(&playerstate->gunangles.values, 0));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_angles_get_char_comp(&playerstate->gunangles.values, 1));
        WRITE_CHECKED(server_write, io_arg, i8,
                      q2proto_var_small_angles_get_char_comp(&playerstate->gunangles.values, 2));
    }

    if (flags & PS_BLEND) {
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 0));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 1));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 2));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&playerstate->blend.values, 3));
    }
    if (flags & PS_FOV)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->fov);
    if (flags & PS_RDFLAGS)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->rdflags);

    // send stats
    if (*extraflags & EPS_STATS) {
        WRITE_CHECKED(server_write, io_arg, u32, playerstate->statbits);
        for (int i = 0; i < 32; i++)
            if (playerstate->statbits & (1 << i))
                WRITE_CHECKED(server_write, io_arg, i16, playerstate->stats[i]);
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                               const q2proto_svc_frame_t *frame)
{
    // save the position for the command byte
    void *command_byte;
    CHECKED_IO(client_write, io_arg, command_byte = q2protoio_write_reserve_raw(io_arg, 1), "reserve command byte");

    // we don't need full 32bits for framenum - 27 gives enough for 155 days on the same map :)
    // could even go lower if we need more bits later
    int32_t encodedFrame = frame->serverframe & 0x07FFFFFF;

    int offset;
    if (frame->deltaframe == -1)
        offset = 31; // special case
    else
        offset = frame->serverframe - frame->deltaframe;

    // first 5 bits of framenum = offset for delta
    encodedFrame |= (offset << 27);

    WRITE_CHECKED(server_write, io_arg, i32, encodedFrame);

    // save the position for the extraflags & suppresscount byte
    void *suppress_count;
    CHECKED_IO(client_write, io_arg, suppress_count = q2protoio_write_reserve_raw(io_arg, 1),
               "reserve suppress_count byte");

    // write areabits
    WRITE_CHECKED(server_write, io_arg, u8, frame->areabits_len);
    void *areabits;
    CHECKED_IO(server_write, io_arg, areabits = q2protoio_write_reserve_raw(io_arg, frame->areabits_len),
               "reserve areabits");
    memcpy(areabits, frame->areabits, frame->areabits_len);

    uint8_t extraflags;
    CHECKED(server_write, io_arg, r1q2_server_write_playerstate(io_arg, &frame->playerstate, &extraflags));

    *(uint8_t *)command_byte = svc_frame | ((extraflags & 0xF0) << 1);
    *(uint8_t *)suppress_count = frame->suppress_count | ((extraflags & 0x0F) << 4);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_write_frame_entity_delta(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                            const q2proto_svc_frame_entity_delta_t *frame_entity_delta)
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

    return r1q2_server_write_entity_state_delta(context, io_arg, frame_entity_delta->newnum,
                                                &frame_entity_delta->entity_delta);
}

#define WRITE_GAMESTATE_FUNCTION_NAME r1q2_server_write_gamestate
#define WRITE_GAMESTATE_BASELINE_SIZE \
    (1   /* command byte */           \
     + 6 /* bits & number */          \
     + 4 /* model indices */          \
     + 2 /* frame */                  \
     + 4 /* skin */                   \
     + 4 /* effects */                \
     + 4 /* renderfx */               \
     + 6 /* origin */                 \
     + 3 /* angles */                 \
     + 6 /* old_origin */             \
     + 1 /* sound */                  \
     + 1 /* event */                  \
     + 4 /* solid */                  \
    )
#define WRITE_GAMESTATE_BASELINE r1q2_server_write_spawnbaseline
#define WRITE_GAMESTATE_ENABLE_DEFLATE

#include "q2proto_write_gamestate.inc"

#undef WRITE_GAMESTATE_FUNCTION_NAME
#undef WRITE_GAMESTATE_BASELINE_SIZE
#undef WRITE_GAMESTATE_BASELINE
#undef WRITE_GAMESTATE_ENABLE_DEFLATE

static q2proto_error_t r1q2_server_read_move(q2proto_servercontext_t *context, uintptr_t io_arg,
                                             q2proto_clc_move_t *move);
static q2proto_error_t r1q2_server_read_setting(uintptr_t io_arg, q2proto_clc_setting_t *setting);

static q2proto_error_t r1q2_server_read(q2proto_servercontext_t *context, uintptr_t io_arg,
                                        q2proto_clc_message_t *clc_message)
{
    memset(clc_message, 0, sizeof(*clc_message));

    size_t command_read = 0;
    const void *command_ptr = NULL;
    READ_CHECKED(client_read, io_arg, command_ptr, raw, 1, &command_read);
    if (command_read == 0)
        return Q2P_ERR_NO_MORE_INPUT;

    uint8_t command = *(const uint8_t *)command_ptr;

    switch (command) {
    case clc_nop:
        clc_message->type = Q2P_CLC_NOP;
        return Q2P_ERR_SUCCESS;

    case clc_move:
        clc_message->type = Q2P_CLC_MOVE;
        return r1q2_server_read_move(context, io_arg, &clc_message->move);

    case clc_userinfo:
        clc_message->type = Q2P_CLC_USERINFO;
        return q2proto_common_server_read_userinfo(io_arg, &clc_message->userinfo);

    case clc_stringcmd:
        clc_message->type = Q2P_CLC_STRINGCMD;
        return q2proto_common_server_read_stringcmd(io_arg, &clc_message->stringcmd);

    case clc_r1q2_setting:
        clc_message->type = Q2P_CLC_SETTING;
        return r1q2_server_read_setting(io_arg, &clc_message->setting);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t r1q2_server_read_move_delta(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                   q2proto_clc_move_delta_t *move_delta)
{
    uint8_t bits;
    READ_CHECKED(server_read, io_arg, bits, u8);

    bool compressed_movements = context->protocol_version >= PROTOCOL_VERSION_R1Q2_UCMD;

    uint8_t buttons = 0;
    if (compressed_movements) {
        if (delta_bits_check(bits, CM_BUTTONS, &move_delta->delta_bits, Q2P_CMD_BUTTONS))
            READ_CHECKED(server_read, io_arg, buttons, u8);
    }

    if (delta_bits_check(bits, CM_ANGLE1, &move_delta->delta_bits, Q2P_CMD_ANGLE0)) {
        if (buttons & BUTTON_UCMD_DBL_ANGLE1) {
            int8_t a;
            READ_CHECKED(server_read, io_arg, a, i8);
            q2proto_var_angles_set_short_comp(&move_delta->angles, 0, a * 64);
        } else
            READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 0);
    }
    if (delta_bits_check(bits, CM_ANGLE2, &move_delta->delta_bits, Q2P_CMD_ANGLE1)) {
        if (buttons & BUTTON_UCMD_DBL_ANGLE2) {
            int8_t a;
            READ_CHECKED(server_read, io_arg, a, i8);
            q2proto_var_angles_set_short_comp(&move_delta->angles, 1, a * 256);
        } else
            READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 1);
    }
    if (delta_bits_check(bits, CM_ANGLE3, &move_delta->delta_bits, Q2P_CMD_ANGLE2))
        READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 2);

    if (delta_bits_check(bits, CM_FORWARD, &move_delta->delta_bits, Q2P_CMD_MOVE_FORWARD)) {
        if (buttons & BUTTON_UCMD_DBLFORWARD) {
            int8_t c;
            READ_CHECKED(server_read, io_arg, c, i8);
            q2proto_var_coords_set_int_unscaled_comp(&move_delta->move, 0, c * 5);
        } else
            READ_CHECKED_VAR_COORDS_COMP_16_UNSCALED(server_read, io_arg, &move_delta->move, 0);
    }
    if (delta_bits_check(bits, CM_SIDE, &move_delta->delta_bits, Q2P_CMD_MOVE_SIDE)) {
        if (buttons & BUTTON_UCMD_DBLSIDE) {
            int8_t c;
            READ_CHECKED(server_read, io_arg, c, i8);
            q2proto_var_coords_set_int_unscaled_comp(&move_delta->move, 1, c * 5);
        } else
            READ_CHECKED_VAR_COORDS_COMP_16_UNSCALED(server_read, io_arg, &move_delta->move, 1);
    }
    if (delta_bits_check(bits, CM_UP, &move_delta->delta_bits, Q2P_CMD_MOVE_UP)) {
        if (buttons & BUTTON_UCMD_DBLUP) {
            int8_t c;
            READ_CHECKED(server_read, io_arg, c, i8);
            q2proto_var_coords_set_int_unscaled_comp(&move_delta->move, 2, c * 5);
        } else
            READ_CHECKED_VAR_COORDS_COMP_16_UNSCALED(server_read, io_arg, &move_delta->move, 2);
    }

    if (!compressed_movements) {
        if (delta_bits_check(bits, CM_BUTTONS, &move_delta->delta_bits, Q2P_CMD_BUTTONS))
            READ_CHECKED(server_read, io_arg, buttons, u8);
    }
    if (delta_bits_check(bits, CM_IMPULSE, &move_delta->delta_bits, Q2P_CMD_IMPULSE))
        READ_CHECKED(server_read, io_arg, move_delta->impulse, u8);

    READ_CHECKED(server_read, io_arg, move_delta->msec, u8);
    READ_CHECKED(server_read, io_arg, move_delta->lightlevel, u8);

    move_delta->buttons = buttons
                          & ~(BUTTON_UCMD_DBLFORWARD | BUTTON_UCMD_DBLSIDE | BUTTON_UCMD_DBLUP | BUTTON_UCMD_DBL_ANGLE1
                              | BUTTON_UCMD_DBL_ANGLE2);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_read_move(q2proto_servercontext_t *context, uintptr_t io_arg,
                                             q2proto_clc_move_t *move)
{
    READ_CHECKED(server_read, io_arg, move->lastframe, i32);
    for (int i = 0; i < 3; i++) {
        CHECKED(server_read, io_arg, r1q2_server_read_move_delta(context, io_arg, &move->moves[i]));
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t r1q2_server_read_setting(uintptr_t io_arg, q2proto_clc_setting_t *setting)
{
    READ_CHECKED(server_read, io_arg, setting->index, i16);
    READ_CHECKED(server_read, io_arg, setting->value, i16);
    return Q2P_ERR_SUCCESS;
}


static q2proto_error_t r1q2_download_begin(q2proto_servercontext_t *context, q2proto_server_download_state_t *state,
                                           q2proto_download_compress_t compress, q2protoio_deflate_args_t *deflate_args)
{
    if (compress == Q2PROTO_DOWNLOAD_COMPRESS_AUTO && Q2PROTO_COMPRESSION_DEFLATE && context->features.enable_deflate
        && state->total_size > 0)
    {
        state->compress = Q2PROTO_DOWNLOAD_DATA_COMPRESS;
        state->deflate_args = deflate_args;
    }
    return Q2P_ERR_SUCCESS;
}

#define SVC_ZDOWNLOAD_SIZE 6

static q2proto_error_t r1q2_download_data(q2proto_server_download_state_t *state, const uint8_t **data,
                                          size_t *remaining, size_t packet_remaining,
                                          q2proto_svc_download_t *svc_download)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    if (state->compress && packet_remaining > SVC_ZDOWNLOAD_SIZE) {
        size_t max_compressed = packet_remaining - SVC_ZDOWNLOAD_SIZE;
        max_compressed = MIN(max_compressed, INT16_MAX);

        if (state->deflate_io_valid) {
            q2protoio_deflate_end(state->deflate_io);
        }

        uintptr_t deflate_io;
        q2proto_error_t err =
            q2protoio_deflate_begin(state->deflate_args, max_compressed, Q2P_INFL_DEFL_RAW, &deflate_io);
        if (err != Q2P_ERR_SUCCESS)
            return err;
        state->deflate_io = deflate_io;
        state->deflate_io_valid = true;

        size_t in_consumed = 0;
        CHECKED_IO(server_write, deflate_io, q2protoio_write_raw(deflate_io, *data, *remaining, &in_consumed),
                   "write download data");
        *data += in_consumed;
        *remaining -= in_consumed;

        const void *compressed_data;
        size_t compressed_size;
        err = q2protoio_deflate_get_data(deflate_io, NULL, &compressed_data, &compressed_size);
        if (err != Q2P_ERR_SUCCESS)
            return err;

        svc_download->compressed = true;
        svc_download->data = compressed_data;
        svc_download->size = compressed_size;
        svc_download->uncompressed_size = in_consumed;

        return q2proto_download_common_complete_struct(state, *remaining, svc_download);
    }
#endif

    return q2proto_download_common_data(state, data, remaining, packet_remaining, svc_download);
}

#undef SVC_ZDOWNLOAD_SIZE
