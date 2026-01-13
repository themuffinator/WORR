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

#include <string.h>

//
// CLIENT: PARSE MESSAGES FROM SERVER
//

static q2proto_error_t vanilla_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                           q2proto_svc_message_t *svc_message);
static q2proto_error_t vanilla_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                              q2proto_svc_frame_entity_delta_t *frame_entity_delta);
static q2proto_error_t vanilla_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                            const q2proto_clc_message_t *clc_message);
static uint32_t vanilla_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                   const q2proto_vec3_t maxs);
static void vanilla_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                                 q2proto_vec3_t maxs);

q2proto_error_t q2proto_vanilla_continue_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                    q2proto_svc_serverdata_t *serverdata)
{
    context->pack_solid = vanilla_pack_solid;
    context->unpack_solid = vanilla_unpack_solid;

    READ_CHECKED(client_read, io_arg, serverdata->servercount, i32);
    READ_CHECKED(client_read, io_arg, serverdata->attractloop, bool);
    READ_CHECKED(client_read, io_arg, serverdata->gamedir, string);
    READ_CHECKED(client_read, io_arg, serverdata->clientnum, i16);
    READ_CHECKED(client_read, io_arg, serverdata->levelname, string);

    context->client_read = vanilla_client_read;
    context->client_write = vanilla_client_write;
    context->server_protocol = serverdata->protocol == PROTOCOL_OLD_DEMO ? Q2P_PROTOCOL_OLD_DEMO : Q2P_PROTOCOL_VANILLA;

    context->features.has_upmove = true;

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                      q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t vanilla_client_read_entity_delta(uintptr_t io_arg, uint64_t bits,
                                                        q2proto_entity_state_delta_t *entity_state);
static q2proto_error_t vanilla_client_read_baseline(uintptr_t io_arg, q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t vanilla_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 q2proto_svc_frame_t *frame);

static MAYBE_UNUSED const char *vanilla_server_cmd_string(int command)
{
    const char *str = q2proto_debug_common_svc_string(command);
    return str ? str : q2proto_va("%d", command);
}

static q2proto_error_t vanilla_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                           q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    size_t command_read = 0;
    const void *command_ptr = NULL;
    READ_CHECKED(client_read, io_arg, command_ptr, raw, 1, &command_read);
    if (command_read == 0)
        return Q2P_ERR_NO_MORE_INPUT;

    uint8_t command = *(const uint8_t *)command_ptr;
    SHOWNET(io_arg, 1, -1, "%s", vanilla_server_cmd_string(command));

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
        return vanilla_client_read_serverdata(context, io_arg, &svc_message->serverdata);

    case svc_configstring:
        svc_message->type = Q2P_SVC_CONFIGSTRING;
        return q2proto_common_client_read_configstring(io_arg, &svc_message->configstring);

    case svc_sound:
        svc_message->type = Q2P_SVC_SOUND;
        return q2proto_common_client_read_sound_short(io_arg, &svc_message->sound);

    case svc_spawnbaseline:
        svc_message->type = Q2P_SVC_SPAWNBASELINE;
        return vanilla_client_read_baseline(io_arg, &svc_message->spawnbaseline);

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
        return vanilla_client_read_frame(context, io_arg, &svc_message->frame);

    case svc_inventory:
        svc_message->type = Q2P_SVC_INVENTORY;
        return q2proto_common_client_read_inventory(io_arg, &svc_message->inventory);

    case svc_layout:
        svc_message->type = Q2P_SVC_LAYOUT;
        return q2proto_common_client_read_layout(io_arg, &svc_message->layout);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t vanilla_client_read_delta_entities(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                          q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    svc_message->type = Q2P_SVC_FRAME_ENTITY_DELTA;
    q2proto_error_t err = vanilla_client_next_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);
    if (err != Q2P_ERR_SUCCESS) {
        // FIXME: May be insufficient, might need some explicit way to reset parsing...
        context->client_read = vanilla_client_read;
        return err;
    }

    if (svc_message->frame_entity_delta.newnum == 0) {
        context->client_read = vanilla_client_read;
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
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

    return vanilla_client_read_entity_delta(io_arg, bits, &frame_entity_delta->entity_delta);
}

static q2proto_error_t vanilla_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                      q2proto_svc_serverdata_t *serverdata)
{
    int32_t protocol;
    READ_CHECKED(client_read, io_arg, protocol, i32);

    if (protocol != PROTOCOL_OLD_DEMO && protocol != PROTOCOL_VANILLA)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "unexpected protocol %d", protocol);

    serverdata->protocol = protocol;

    return q2proto_vanilla_continue_serverdata(context, io_arg, serverdata);
}

static q2proto_error_t vanilla_client_read_entity_delta(uintptr_t io_arg, uint64_t bits,
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

    if (delta_bits_check(bits, U_SOLID, &entity_state->delta_bits, Q2P_ESD_SOLID))
        READ_CHECKED(client_read, io_arg, entity_state->solid, u16);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_client_read_baseline(uintptr_t io_arg, q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &spawnbaseline->entnum));

    q2proto_debug_shownet_entity_delta_bits(io_arg, "   baseline:", spawnbaseline->entnum, bits);

    return vanilla_client_read_entity_delta(io_arg, bits, &spawnbaseline->delta_state);
}

static q2proto_error_t vanilla_client_read_playerstate(uintptr_t io_arg, q2proto_svc_playerstate_t *playerstate)
{
    uint16_t flags;
    READ_CHECKED(client_read, io_arg, flags, u16);

#if Q2PROTO_SHOWNET
    if (q2protodbg_shownet_check(io_arg, 2) && flags) {
        char buf[1024];
        q2proto_debug_common_player_delta_bits_to_str(buf, sizeof(buf), flags);
        SHOWNET(io_arg, 2, -2, "   %s", buf);
    }
#endif

    //
    // parse the pmove_state_t
    //
    if (delta_bits_check(flags, PS_M_TYPE, &playerstate->delta_bits, Q2P_PSD_PM_TYPE))
        READ_CHECKED(client_read, io_arg, playerstate->pm_type, u8);

    if (flags & PS_M_ORIGIN) {
        CHECKED(client_read, io_arg, read_var_coords_short(io_arg, &playerstate->pm_origin.read.value.values));
        playerstate->pm_origin.read.value.delta_bits = 0x7;
    }

    if (flags & PS_M_VELOCITY) {
        CHECKED(client_read, io_arg, read_var_coords_short(io_arg, &playerstate->pm_velocity.read.value.values));
        playerstate->pm_velocity.read.value.delta_bits = 0x7;
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

    if (flags & PS_VIEWANGLES) {
        CHECKED(client_read, io_arg, read_var_angles16(io_arg, &playerstate->viewangles.values));
        playerstate->viewangles.delta_bits = 0x7;
    }

    if (delta_bits_check(flags, PS_KICKANGLES, &playerstate->delta_bits, Q2P_PSD_KICKANGLES))
        CHECKED(client_read, io_arg, read_var_small_angles(io_arg, &playerstate->kick_angles));

    if (delta_bits_check(flags, PS_WEAPONINDEX, &playerstate->delta_bits, Q2P_PSD_GUNINDEX))
        READ_CHECKED(client_read, io_arg, playerstate->gunindex, u8);

    if (delta_bits_check(flags, PS_WEAPONFRAME, &playerstate->delta_bits, Q2P_PSD_GUNFRAME)) {
        READ_CHECKED(client_read, io_arg, playerstate->gunframe, u8);
        CHECKED(client_read, io_arg, read_var_small_offsets(io_arg, &playerstate->gunoffset.values));
        playerstate->gunoffset.delta_bits = BIT(0) | BIT(1) | BIT(2);
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
    READ_CHECKED(client_read, io_arg, playerstate->statbits, u32);
    for (int i = 0; i < 32; i++)
        if (playerstate->statbits & (1 << i))
            READ_CHECKED(client_read, io_arg, playerstate->stats[i], i16);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 q2proto_svc_frame_t *frame)
{
    READ_CHECKED(client_read, io_arg, frame->serverframe, i32);
    READ_CHECKED(client_read, io_arg, frame->deltaframe, i32);

    // BIG HACK to let old demos continue to work
    if (context->server_protocol != Q2P_PROTOCOL_OLD_DEMO)
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
    CHECKED(client_read, io_arg, vanilla_client_read_playerstate(io_arg, &frame->playerstate));

    // read packet entities
    READ_CHECKED(client_read, io_arg, cmd, u8);
    if (cmd != svc_packetentities)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "%s: expected packetentities, got %d", __func__,
                            cmd);
    context->client_read = vanilla_client_read_delta_entities;
    SHOWNET(io_arg, 2, -1, "packetentities");

    return Q2P_ERR_SUCCESS;
}

//
// CLIENT: SEND MESSAGES TO SERVER
//

static q2proto_error_t vanilla_client_write_move(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 const q2proto_clc_move_t *move);

static q2proto_error_t vanilla_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                            const q2proto_clc_message_t *clc_message)
{
    switch (clc_message->type) {
    case Q2P_CLC_NOP:
        return q2proto_common_client_write_nop(io_arg);

    case Q2P_CLC_MOVE:
        return vanilla_client_write_move(context, io_arg, &clc_message->move);

    case Q2P_CLC_USERINFO:
        return q2proto_common_client_write_userinfo(io_arg, &clc_message->userinfo);

    case Q2P_CLC_STRINGCMD:
        return q2proto_common_client_write_stringcmd(io_arg, &clc_message->stringcmd);

    default:
        break;
    }

    return Q2P_ERR_BAD_COMMAND;
}

static q2proto_error_t vanilla_client_write_move_delta(uintptr_t io_arg, const q2proto_clc_move_delta_t *move_delta)
{
    uint8_t bits = 0;
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
    if (move_delta->delta_bits & Q2P_CMD_BUTTONS)
        bits |= CM_BUTTONS;
    if (move_delta->delta_bits & Q2P_CMD_IMPULSE)
        bits |= CM_IMPULSE;

    WRITE_CHECKED(client_write, io_arg, u8, bits);

    if (bits & CM_ANGLE1)
        WRITE_CHECKED(client_write, io_arg, i16, q2proto_var_angles_get_short_comp(&move_delta->angles, 0));
    if (bits & CM_ANGLE2)
        WRITE_CHECKED(client_write, io_arg, i16, q2proto_var_angles_get_short_comp(&move_delta->angles, 1));
    if (bits & CM_ANGLE3)
        WRITE_CHECKED(client_write, io_arg, i16, q2proto_var_angles_get_short_comp(&move_delta->angles, 2));

    if (bits & CM_FORWARD)
        WRITE_CHECKED(client_write, io_arg, i16, q2proto_var_coords_get_short_unscaled_comp(&move_delta->move, 0));
    if (bits & CM_SIDE)
        WRITE_CHECKED(client_write, io_arg, i16, q2proto_var_coords_get_short_unscaled_comp(&move_delta->move, 1));
    if (bits & CM_UP)
        WRITE_CHECKED(client_write, io_arg, i16, q2proto_var_coords_get_short_unscaled_comp(&move_delta->move, 2));

    if (bits & CM_BUTTONS)
        WRITE_CHECKED(client_write, io_arg, u8, move_delta->buttons);
    if (bits & CM_IMPULSE)
        WRITE_CHECKED(client_write, io_arg, u8, move_delta->impulse);

    WRITE_CHECKED(client_write, io_arg, u8, move_delta->msec);
    WRITE_CHECKED(client_write, io_arg, u8, move_delta->lightlevel);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_client_write_move(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 const q2proto_clc_move_t *move)
{
    WRITE_CHECKED(client_write, io_arg, u8, clc_move);

    // save the position for a checksum byte
    void *checksum_byte;
    CHECKED_IO(client_write, io_arg, checksum_byte = q2protoio_write_reserve_raw(io_arg, 1), "reserve checksum byte");

    WRITE_CHECKED(client_write, io_arg, i32, move->lastframe);

    for (int i = 0; i < 3; i++) {
        CHECKED(client_write, io_arg, vanilla_client_write_move_delta(io_arg, &move->moves[i]));
    }

    uint8_t *checksum_start = (uint8_t *)checksum_byte + 1;
    uint8_t *checksum_end;
    CHECKED_IO(client_write, io_arg, checksum_end = (uint8_t *)q2protoio_write_reserve_raw(io_arg, 0),
               "get checksum end");

    CHECKED(client_write, io_arg,
            q2proto_block_sequence_crc_byte(checksum_start, checksum_end - checksum_start, move->sequence,
                                            (uint8_t *)checksum_byte));
    return Q2P_ERR_SUCCESS;
}

static uint32_t vanilla_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                   const q2proto_vec3_t maxs)
{
    return q2proto_pack_solid_16(mins, maxs);
}

static void vanilla_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                                 q2proto_vec3_t maxs)
{
    q2proto_unpack_solid_16(solid, mins, maxs);
}

//
// SERVER: INITIALIZATION
//
static q2proto_error_t vanilla_server_fill_serverdata(q2proto_servercontext_t *context,
                                                      q2proto_svc_serverdata_t *serverdata);
static void vanilla_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_entity_state_t *from,
                                                   const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                   q2proto_entity_state_delta_t *delta);
static void vanilla_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_player_state_t *from,
                                                   const q2proto_packed_player_state_t *to,
                                                   q2proto_svc_playerstate_t *delta);
static q2proto_error_t vanilla_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                            const q2proto_svc_message_t *svc_message);
static q2proto_error_t vanilla_server_write_gamestate(q2proto_servercontext_t *context,
                                                      q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                      const q2proto_gamestate_t *gamestate);
static q2proto_error_t vanilla_server_read(q2proto_servercontext_t *context, uintptr_t io_arg,
                                           q2proto_clc_message_t *clc_message);

static const struct q2proto_download_funcs_s vanilla_download_funcs = {.begin = NULL,
                                                                       .data = q2proto_download_common_data,
                                                                       .finish = q2proto_download_common_finish,
                                                                       .abort = q2proto_download_common_abort};

q2proto_error_t q2proto_vanilla_init_servercontext(q2proto_servercontext_t *context,
                                                   const q2proto_connect_t *connect_info)
{
    if (context->server_info->game_api != Q2PROTO_GAME_VANILLA)
        return Q2P_ERR_GAMETYPE_UNSUPPORTED;

    context->fill_serverdata = vanilla_server_fill_serverdata;
    context->make_entity_state_delta = vanilla_server_make_entity_state_delta;
    context->make_player_state_delta = vanilla_server_make_player_state_delta;
    context->server_write = vanilla_server_write;
    context->server_write_gamestate = vanilla_server_write_gamestate;
    context->server_read = vanilla_server_read;
    context->download_funcs = &vanilla_download_funcs;
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_fill_serverdata(q2proto_servercontext_t *context,
                                                      q2proto_svc_serverdata_t *serverdata)
{
    serverdata->protocol = PROTOCOL_VANILLA;
    return Q2P_ERR_SUCCESS;
}

static void vanilla_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_entity_state_t *from,
                                                   const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                   q2proto_entity_state_delta_t *delta)
{
    q2proto_packing_make_entity_state_delta(from, to, write_old_origin, false, delta);
}

static void vanilla_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_player_state_t *from,
                                                   const q2proto_packed_player_state_t *to,
                                                   q2proto_svc_playerstate_t *delta)
{
    q2proto_packing_make_player_state_delta(from, to, delta);
}

static q2proto_error_t vanilla_server_write_serverdata(uintptr_t io_arg, const q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t vanilla_server_write_spawnbaseline(uintptr_t io_arg,
                                                          const q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t vanilla_server_write_download(uintptr_t io_arg, const q2proto_svc_download_t *download);
static q2proto_error_t vanilla_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                  const q2proto_svc_frame_t *frame);
static q2proto_error_t vanilla_server_write_frame_entity_delta(
    uintptr_t io_arg, const q2proto_svc_frame_entity_delta_t *frame_entity_delta);

static q2proto_error_t vanilla_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
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
        return vanilla_server_write_serverdata(io_arg, &svc_message->serverdata);

    case Q2P_SVC_CONFIGSTRING:
        return q2proto_common_server_write_configstring(io_arg, &svc_message->configstring);

    case Q2P_SVC_SPAWNBASELINE:
        return vanilla_server_write_spawnbaseline(io_arg, &svc_message->spawnbaseline);

    case Q2P_SVC_CENTERPRINT:
        return q2proto_common_server_write_centerprint(io_arg, &svc_message->centerprint);

    case Q2P_SVC_DOWNLOAD:
        return vanilla_server_write_download(io_arg, &svc_message->download);

    case Q2P_SVC_FRAME:
        return vanilla_server_write_frame(context, io_arg, &svc_message->frame);

    case Q2P_SVC_FRAME_ENTITY_DELTA:
        return vanilla_server_write_frame_entity_delta(io_arg, &svc_message->frame_entity_delta);

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

static q2proto_error_t vanilla_server_write_serverdata(uintptr_t io_arg, const q2proto_svc_serverdata_t *serverdata)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_serverdata);
    WRITE_CHECKED(server_write, io_arg, i32, PROTOCOL_VANILLA);
    WRITE_CHECKED(server_write, io_arg, i32, serverdata->servercount);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->attractloop);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->gamedir);
    WRITE_CHECKED(server_write, io_arg, i16, serverdata->clientnum);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->levelname);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_write_entity_state_delta(uintptr_t io_arg, uint16_t entnum,
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
    if (bits & U_SOLID)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->solid);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_write_spawnbaseline(uintptr_t io_arg,
                                                          const q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_spawnbaseline);
    CHECKED(server_write, io_arg,
            vanilla_server_write_entity_state_delta(io_arg, spawnbaseline->entnum, &spawnbaseline->delta_state));
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_write_download(uintptr_t io_arg, const q2proto_svc_download_t *download)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_download);
    WRITE_CHECKED(server_write, io_arg, i16, download->size);
    WRITE_CHECKED(server_write, io_arg, u8, download->percent);
    if (download->size > 0) {
        void *p;
        CHECKED_IO(server_write, io_arg, p = q2protoio_write_reserve_raw(io_arg, download->size),
                   "reserve download data");
        memcpy(p, download->data, download->size);
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_write_playerstate(uintptr_t io_arg, const q2proto_svc_playerstate_t *playerstate)
{
    uint16_t flags = 0;

    if (playerstate->delta_bits & Q2P_PSD_PM_TYPE)
        flags |= PS_M_TYPE;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_origin))
        flags |= PS_M_ORIGIN;
    if (q2proto_maybe_diff_coords_write_differs_int(&playerstate->pm_velocity))
        flags |= PS_M_VELOCITY;
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
    if (playerstate->viewangles.delta_bits != 0)
        flags |= PS_VIEWANGLES;
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
    if ((playerstate->delta_bits & Q2P_PSD_GUNFRAME) || (playerstate->gunoffset.delta_bits != 0)
        || (playerstate->gunangles.delta_bits != 0))
    {
        flags |= PS_WEAPONFRAME;
        if (playerstate->gunframe > UINT8_MAX)
            return Q2P_ERR_BAD_DATA;
    }
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
    WRITE_CHECKED(server_write, io_arg, u8, svc_playerinfo);
    WRITE_CHECKED(server_write, io_arg, u16, flags);

    if (flags & PS_M_TYPE)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->pm_type);

    if (flags & PS_M_ORIGIN)
        WRITE_CHECKED(server_write, io_arg, var_coords_short, &playerstate->pm_origin.write.current);

    if (flags & PS_M_VELOCITY)
        WRITE_CHECKED(server_write, io_arg, var_coords_short, &playerstate->pm_velocity.write.current);

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
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 2));
    }

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
    WRITE_CHECKED(server_write, io_arg, u32, playerstate->statbits);
    for (int i = 0; i < 32; i++)
        if (playerstate->statbits & (1 << i))
            WRITE_CHECKED(server_write, io_arg, i16, playerstate->stats[i]);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
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

    CHECKED(server_write, io_arg, vanilla_server_write_playerstate(io_arg, &frame->playerstate));

    WRITE_CHECKED(server_write, io_arg, u8, svc_packetentities);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_write_frame_entity_delta(
    uintptr_t io_arg, const q2proto_svc_frame_entity_delta_t *frame_entity_delta)
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

    return vanilla_server_write_entity_state_delta(io_arg, frame_entity_delta->newnum,
                                                   &frame_entity_delta->entity_delta);
}

#define WRITE_GAMESTATE_FUNCTION_NAME vanilla_server_write_gamestate
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
     + 2 /* solid */                  \
    )
#define WRITE_GAMESTATE_BASELINE(C, I, S) vanilla_server_write_spawnbaseline(I, S)

#include "q2proto_write_gamestate.inc"

static q2proto_error_t vanilla_server_read_move(uintptr_t io_arg, q2proto_clc_move_t *move);

static q2proto_error_t vanilla_server_read(q2proto_servercontext_t *context, uintptr_t io_arg,
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
        return vanilla_server_read_move(io_arg, &clc_message->move);

    case clc_userinfo:
        clc_message->type = Q2P_CLC_USERINFO;
        return q2proto_common_server_read_userinfo(io_arg, &clc_message->userinfo);

    case clc_stringcmd:
        clc_message->type = Q2P_CLC_STRINGCMD;
        return q2proto_common_server_read_stringcmd(io_arg, &clc_message->stringcmd);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t vanilla_server_read_move_delta(uintptr_t io_arg, q2proto_clc_move_delta_t *move_delta)
{
    uint8_t bits;
    READ_CHECKED(server_read, io_arg, bits, u8);

    if (delta_bits_check(bits, CM_ANGLE1, &move_delta->delta_bits, Q2P_CMD_ANGLE0))
        READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 0);
    if (delta_bits_check(bits, CM_ANGLE2, &move_delta->delta_bits, Q2P_CMD_ANGLE1))
        READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 1);
    if (delta_bits_check(bits, CM_ANGLE3, &move_delta->delta_bits, Q2P_CMD_ANGLE2))
        READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 2);

    if (delta_bits_check(bits, CM_FORWARD, &move_delta->delta_bits, Q2P_CMD_MOVE_FORWARD))
        READ_CHECKED_VAR_COORDS_COMP_16_UNSCALED(server_read, io_arg, &move_delta->move, 0);
    if (delta_bits_check(bits, CM_SIDE, &move_delta->delta_bits, Q2P_CMD_MOVE_SIDE))
        READ_CHECKED_VAR_COORDS_COMP_16_UNSCALED(server_read, io_arg, &move_delta->move, 1);
    if (delta_bits_check(bits, CM_UP, &move_delta->delta_bits, Q2P_CMD_MOVE_UP))
        READ_CHECKED_VAR_COORDS_COMP_16_UNSCALED(server_read, io_arg, &move_delta->move, 2);

    if (delta_bits_check(bits, CM_BUTTONS, &move_delta->delta_bits, Q2P_CMD_BUTTONS))
        READ_CHECKED(client_write, io_arg, move_delta->buttons, u8);
    if (delta_bits_check(bits, CM_IMPULSE, &move_delta->delta_bits, Q2P_CMD_IMPULSE))
        READ_CHECKED(client_write, io_arg, move_delta->impulse, u8);

    READ_CHECKED(client_write, io_arg, move_delta->msec, u8);
    READ_CHECKED(client_write, io_arg, move_delta->lightlevel, u8);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t vanilla_server_read_move(uintptr_t io_arg, q2proto_clc_move_t *move)
{
    CHECKED_IO(server_read, io_arg, q2protoio_read_u8(io_arg), "skip checksum byte");

    READ_CHECKED(server_read, io_arg, move->lastframe, i32);

    for (int i = 0; i < 3; i++) {
        CHECKED(server_read, io_arg, vanilla_server_read_move_delta(io_arg, &move->moves[i]));
    }

    return Q2P_ERR_SUCCESS;
}
