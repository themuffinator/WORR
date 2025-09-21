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
#include "q2proto_internal_bit_read_write.h"

/* Initial Q2rePRO protocol version; matched Q2PRO "current" version at time
 * of forking. */
#define Q2REPRO_PROTOCOL_VERSION_MINIMUM 1024
#define Q2REPRO_PROTOCOL_VERSION_CURRENT Q2REPRO_PROTOCOL_VERSION_MINIMUM

q2proto_error_t q2proto_q2repro_parse_connect(q2proto_string_t *connect_str, q2proto_connect_t *parsed_connect)
{
    parsed_connect->q2pro_nctype = 1; // NETCHAN_NEW

    q2proto_string_t zlib_token = {0};
    next_token(&zlib_token, connect_str, ' ');
    parsed_connect->has_zlib = q2pstol(&zlib_token, 10) != 0;

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_q2repro_complete_connect(q2proto_connect_t *connect)
{
    if (connect->version == 0)
        connect->version = Q2REPRO_PROTOCOL_VERSION_CURRENT;
    connect->has_zlib = Q2PROTO_COMPRESSION_DEFLATE;
    connect->qport &= 0xff;
    connect->q2pro_nctype = 1; // force new netchan
    return Q2P_ERR_SUCCESS;
}

const char *q2proto_q2repro_connect_tail(const q2proto_connect_t *connect)
{
    return q2proto_va("%d %d", connect->packet_length, connect->has_zlib);
}

//
// CLIENT: PARSE MESSAGES FROM SERVER
//

#define READ_CHECKED_GUNOFFSET_COMP(SOURCE, IO_ARG, TARGET, COMP)              \
    do {                                                                       \
        int16_t o;                                                             \
        READ_CHECKED(SOURCE, (IO_ARG), o, i16);                                \
        q2proto_var_small_offsets_set_q2repro_gunoffset_comp(TARGET, COMP, o); \
    } while (0)

static inline q2proto_error_t read_short_gunoffset(uintptr_t io_arg, q2proto_var_small_offsets_t *offs)
{
    READ_CHECKED_GUNOFFSET_COMP(client_read, io_arg, offs, 0);
    READ_CHECKED_GUNOFFSET_COMP(client_read, io_arg, offs, 1);
    READ_CHECKED_GUNOFFSET_COMP(client_read, io_arg, offs, 2);
    return Q2P_ERR_SUCCESS;
}

#define READ_CHECKED_GUNANGLES_COMP(SOURCE, IO_ARG, TARGET, COMP)             \
    do {                                                                      \
        int16_t a;                                                            \
        READ_CHECKED(SOURCE, (IO_ARG), a, i16);                               \
        q2proto_var_small_angles_set_q2repro_gunangles_comp(TARGET, COMP, a); \
    } while (0)

static inline q2proto_error_t read_short_gunangles(uintptr_t io_arg, q2proto_var_small_angles_t *angles)
{
    READ_CHECKED_GUNANGLES_COMP(client_read, io_arg, angles, 0);
    READ_CHECKED_GUNANGLES_COMP(client_read, io_arg, angles, 1);
    READ_CHECKED_GUNANGLES_COMP(client_read, io_arg, angles, 2);
    return Q2P_ERR_SUCCESS;
}

// global fog
#define FOG_RR_BIT_DENSITY BIT(0)
#define FOG_RR_BIT_R       BIT(1)
#define FOG_RR_BIT_G       BIT(2)
#define FOG_RR_BIT_B       BIT(3)
#define FOG_RR_BIT_TIME    BIT(4) // if set, the transition takes place over N milliseconds

// height fog
#define FOG_RR_BIT_HEIGHTFOG_FALLOFF    BIT(5)
#define FOG_RR_BIT_HEIGHTFOG_DENSITY    BIT(6)
#define FOG_RR_BIT_MORE_BITS            BIT(7) // read additional bit
#define FOG_RR_BIT_HEIGHTFOG_START_R    BIT(8)
#define FOG_RR_BIT_HEIGHTFOG_START_G    BIT(9)
#define FOG_RR_BIT_HEIGHTFOG_START_B    BIT(10)
#define FOG_RR_BIT_HEIGHTFOG_START_DIST BIT(11)
#define FOG_RR_BIT_HEIGHTFOG_END_R      BIT(12)
#define FOG_RR_BIT_HEIGHTFOG_END_G      BIT(13)
#define FOG_RR_BIT_HEIGHTFOG_END_B      BIT(14)
#define FOG_RR_BIT_HEIGHTFOG_END_DIST   BIT(15)

static q2proto_error_t q2repro_client_read(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                           q2proto_svc_message_t *svc_message);
static q2proto_error_t q2repro_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                              q2proto_svc_frame_entity_delta_t *frame_entity_delta);
static q2proto_error_t q2repro_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                            const q2proto_clc_message_t *clc_message);
static uint32_t q2repro_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                   const q2proto_vec3_t maxs);
static void q2repro_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                                 q2proto_vec3_t maxs);

q2proto_error_t q2proto_q2repro_continue_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                    q2proto_svc_serverdata_t *serverdata)
{
    context->pack_solid = q2repro_pack_solid;
    context->unpack_solid = q2repro_unpack_solid;

    READ_CHECKED(client_read, io_arg, serverdata->servercount, i32);
    READ_CHECKED(client_read, io_arg, serverdata->attractloop, bool);
    READ_CHECKED(client_read, io_arg, serverdata->gamedir, string);
    READ_CHECKED(client_read, io_arg, serverdata->clientnum, i16);
    READ_CHECKED(client_read, io_arg, serverdata->levelname, string);

    READ_CHECKED(client_read, io_arg, serverdata->protocol_version, u16);
    READ_CHECKED(client_read, io_arg, serverdata->q2pro.server_state, u8);
    uint16_t q2repro_flags;
    READ_CHECKED(client_read, io_arg, q2repro_flags, u16);
    serverdata->strafejump_hack = q2repro_flags & Q2PRO_PF_STRAFEJUMP_HACK;
    serverdata->q2pro.qw_mode = q2repro_flags & Q2PRO_PF_QW_MODE;
    serverdata->q2pro.waterjump_hack = q2repro_flags & Q2PRO_PF_WATERJUMP_HACK;
    serverdata->q2pro.extensions = q2repro_flags & Q2PRO_PF_EXTENSIONS;
    serverdata->q2pro.extensions_v2 = q2repro_flags & Q2PRO_PF_EXTENSIONS_2;
    serverdata->q2repro.game3_compat = q2repro_flags & Q2REPRO_PF_GAME3_COMPAT;
    READ_CHECKED(client_read, io_arg, serverdata->q2repro.server_fps, u8);
    if (serverdata->attractloop) {
        /* HACK: server_fps for demos was computed using floating point math,
         * leading to some rounding errors.
         * Round FPS to nearest multiple of 10 */
        serverdata->q2repro.server_fps = (((int)serverdata->q2repro.server_fps + 5) / 10) * 10;
    }

    context->client_read = q2repro_client_read;
    context->client_write = q2repro_client_write;
    context->server_protocol = Q2P_PROTOCOL_Q2REPRO;
    context->protocol_version = serverdata->protocol_version;
    context->features.batch_move = true;
    context->features.userinfo_delta = true;
    context->features.has_clientnum = true;
    context->features.has_solid32 = true;
    if (serverdata->q2repro.game3_compat) {
        // FIXME: correct?
        if (serverdata->q2pro.extensions_v2)
            context->features.server_game_api = Q2PROTO_GAME_Q2PRO_EXTENDED_V2;
        else if (serverdata->q2pro.extensions)
            context->features.server_game_api = Q2PROTO_GAME_Q2PRO_EXTENDED;
        else
            context->features.server_game_api = Q2PROTO_GAME_VANILLA;
    } else
        context->features.server_game_api = Q2PROTO_GAME_RERELEASE;

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                      q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t q2repro_client_read_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                        uint64_t bits, q2proto_entity_state_delta_t *entity_state);
static q2proto_error_t q2repro_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                    q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t q2repro_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                 q2proto_svc_frame_t *frame);
static q2proto_error_t q2repro_client_read_zdownload(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                     q2proto_svc_download_t *download);
static q2proto_error_t q2repro_client_read_begin_gamestate(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                           q2proto_svc_message_t *svc_message);
static q2proto_error_t q2repro_client_read_begin_configstream(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                              q2proto_svc_message_t *svc_message);
static q2proto_error_t q2repro_client_read_begin_baselinestream(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                                q2proto_svc_message_t *svc_message);

static MAYBE_UNUSED const char *q2repro_server_cmd_string(int command)
{
#define S(X) \
    case X:  \
        return #X;

    switch (command) {
        S(svc_q2repro_baselinestream)
        S(svc_q2repro_configstringstream)
        S(svc_q2repro_gamestate)
        S(svc_q2repro_setting)
        S(svc_q2repro_zdownload)
        S(svc_q2repro_zpacket)
        S(svc_rr_achievement)
        S(svc_rr_damage)
        S(svc_rr_help_path)
        S(svc_rr_fog)
        S(svc_rr_muzzleflash3)
        S(svc_rr_poi)
    }

#undef S

    const char *str = q2proto_debug_common_svc_string(command);
    return str ? str : q2proto_va("%d", command);
}

static MAYBE_UNUSED void debug_player_delta_bits_to_str(char *buf, size_t size, uint32_t bits)
{
    q2proto_debug_common_player_delta_bits_to_str(buf, size, bits & ~PS_RR_VIEWHEIGHT);
    size -= strlen(buf);
    buf += strlen(buf);

#define S(b, s)                                         \
    if (bits & PS_##b) {                                \
        q2proto_snprintf_update(&buf, &size, " %s", s); \
        bits &= ~PS_##b;                                \
    }

    S(RR_VIEWHEIGHT, "viewheight");
#undef S
}

static q2proto_error_t q2repro_client_read(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
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
            return q2repro_client_read(context, raw_io_arg, svc_message);
        }
#endif
        return Q2P_ERR_NO_MORE_INPUT;
    }

    uint8_t command = *(const uint8_t *)command_ptr;
    SHOWNET(io_arg, 1, -1, "%s", q2repro_server_cmd_string(command));

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
        return q2repro_client_read_serverdata(context, io_arg, &svc_message->serverdata);

    case svc_configstring:
        svc_message->type = Q2P_SVC_CONFIGSTRING;
        return q2proto_common_client_read_configstring(io_arg, &svc_message->configstring);

    case svc_sound:
        svc_message->type = Q2P_SVC_SOUND;
        return q2proto_common_client_read_sound_float(io_arg, &svc_message->sound);

    case svc_spawnbaseline:
        svc_message->type = Q2P_SVC_SPAWNBASELINE;
        return q2repro_client_read_baseline(context, io_arg, &svc_message->spawnbaseline);

    case svc_temp_entity:
        svc_message->type = Q2P_SVC_TEMP_ENTITY;
        return q2proto_common_client_read_temp_entity_float(io_arg, context->features.server_game_api,
                                                            &svc_message->temp_entity);

    case svc_muzzleflash:
        svc_message->type = Q2P_SVC_MUZZLEFLASH;
        return q2proto_common_client_read_muzzleflash(io_arg, &svc_message->muzzleflash, MZ_SILENCED);

    case svc_muzzleflash2:
        svc_message->type = Q2P_SVC_MUZZLEFLASH2;
        return q2proto_q2pro_client_read_muzzleflash2(context, io_arg, &svc_message->muzzleflash);

    case svc_rr_muzzleflash3:
        svc_message->type = Q2P_SVC_MUZZLEFLASH2;
        return q2proto_q2repro_client_read_muzzleflash3(io_arg, &svc_message->muzzleflash);

    case svc_download:
        svc_message->type = Q2P_SVC_DOWNLOAD;
        return q2proto_common_client_read_download(io_arg, &svc_message->download);

    case svc_frame:
        svc_message->type = Q2P_SVC_FRAME;
        return q2repro_client_read_frame(context, io_arg, &svc_message->frame);

    case svc_inventory:
        svc_message->type = Q2P_SVC_INVENTORY;
        return q2proto_common_client_read_inventory(io_arg, &svc_message->inventory);

    case svc_layout:
        svc_message->type = Q2P_SVC_LAYOUT;
        return q2proto_common_client_read_layout(io_arg, &svc_message->layout);

    case svc_rr_damage:
        svc_message->type = Q2P_SVC_DAMAGE;
        return q2proto_q2repro_client_read_damage(io_arg, &svc_message->damage);

    case svc_rr_fog:
        svc_message->type = Q2P_SVC_FOG;
        return q2proto_q2repro_client_read_fog(io_arg, &svc_message->fog);

    case svc_rr_poi:
        svc_message->type = Q2P_SVC_POI;
        return q2proto_q2repro_client_read_poi(io_arg, &svc_message->poi);

    case svc_rr_help_path:
        svc_message->type = Q2P_SVC_HELP_PATH;
        return q2proto_q2repro_client_read_help_path(io_arg, &svc_message->help_path);

    case svc_rr_achievement:
        svc_message->type = Q2P_SVC_ACHIEVEMENT;
        return q2proto_q2repro_client_read_achievement(io_arg, &svc_message->achievement);

    case svc_q2repro_zpacket:
        CHECKED(client_read, io_arg, r1q2_client_read_zpacket(context, io_arg, svc_message));
        // Call recursively to pick up first message from zpacket
        return q2repro_client_read(context, raw_io_arg, svc_message);

    case svc_q2repro_zdownload:
        svc_message->type = Q2P_SVC_DOWNLOAD;
        return q2repro_client_read_zdownload(context, io_arg, &svc_message->download);

    case svc_q2repro_setting:
        svc_message->type = Q2P_SVC_SETTING;
        return r1q2_client_read_setting(io_arg, &svc_message->setting);

    case svc_q2repro_gamestate:
        return q2repro_client_read_begin_gamestate(context, raw_io_arg, svc_message);

    case svc_q2repro_configstringstream:
        return q2repro_client_read_begin_configstream(context, raw_io_arg, svc_message);

    case svc_q2repro_baselinestream:
        return q2repro_client_read_begin_baselinestream(context, raw_io_arg, svc_message);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t q2repro_client_read_delta_entities(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                          q2proto_svc_message_t *svc_message)
{
    memset(svc_message, 0, sizeof(*svc_message));

    // zpacket might contain multiple packets, so try to read from inflated message repeatedly
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    svc_message->type = Q2P_SVC_FRAME_ENTITY_DELTA;
    q2proto_error_t err = q2repro_client_next_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);
    if (err != Q2P_ERR_SUCCESS) {
        // FIXME: May be insufficient, might need some explicit way to reset parsing...
        context->client_read = q2repro_client_read;
        return err;
    }

    if (svc_message->frame_entity_delta.newnum == 0) {
        context->client_read = q2repro_client_read;
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_next_frame_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                              q2proto_svc_frame_entity_delta_t *frame_entity_delta)
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

    return q2repro_client_read_entity_delta(context, io_arg, bits, &frame_entity_delta->entity_delta);
}

static q2proto_error_t q2repro_client_read_serverdata(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                      q2proto_svc_serverdata_t *serverdata)
{
    int32_t protocol;
    READ_CHECKED(client_read, io_arg, protocol, i32);

    if (protocol != PROTOCOL_Q2REPRO)
        return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_DATA, "unexpected protocol %d", protocol);

    serverdata->protocol = protocol;

    return q2proto_q2repro_continue_serverdata(context, io_arg, serverdata);
}

static q2proto_error_t q2repro_client_read_entity_delta(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                        uint64_t bits, q2proto_entity_state_delta_t *entity_state)
{
    bool model16 = bits & U_MODEL16;
    if (delta_bits_check(bits, U_MODEL, &entity_state->delta_bits, Q2P_ESD_MODELINDEX)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex, u8);
    }
    if (delta_bits_check(bits, U_MODEL2, &entity_state->delta_bits, Q2P_ESD_MODELINDEX2)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex2, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex2, u8);
    }
    if (delta_bits_check(bits, U_MODEL3, &entity_state->delta_bits, Q2P_ESD_MODELINDEX3)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex3, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex3, u8);
    }
    if (delta_bits_check(bits, U_MODEL4, &entity_state->delta_bits, Q2P_ESD_MODELINDEX4)) {
        if (model16)
            READ_CHECKED(client_read, io_arg, entity_state->modelindex4, u16);
        else
            READ_CHECKED(client_read, io_arg, entity_state->modelindex4, u8);
    }

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
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &entity_state->origin.read.value.values, 0);
        entity_state->origin.read.value.delta_bits |= BIT(0);
    }
    if (bits & U_ORIGIN2) {
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &entity_state->origin.read.value.values, 1);
        entity_state->origin.read.value.delta_bits |= BIT(1);
    }
    if (bits & U_ORIGIN3) {
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &entity_state->origin.read.value.values, 2);
        entity_state->origin.read.value.delta_bits |= BIT(2);
    }

    entity_state->angle.delta_bits = 0;
    if (bits & U_ANGLE16) {
        if (bits & U_ANGLE1) {
            READ_CHECKED_VAR_ANGLES_COMP_16(client_read, io_arg, &entity_state->angle.values, 0);
            entity_state->angle.delta_bits |= BIT(0);
        }
        if (bits & U_ANGLE2) {
            READ_CHECKED_VAR_ANGLES_COMP_16(client_read, io_arg, &entity_state->angle.values, 1);
            entity_state->angle.delta_bits |= BIT(1);
        }
        if (bits & U_ANGLE3) {
            READ_CHECKED_VAR_ANGLES_COMP_16(client_read, io_arg, &entity_state->angle.values, 2);
            entity_state->angle.delta_bits |= BIT(2);
        }
    } else {
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
    }

    if (delta_bits_check(bits, U_OLDORIGIN, &entity_state->delta_bits, Q2P_ESD_OLD_ORIGIN))
        CHECKED(client_read, io_arg, read_var_coords_float(io_arg, &entity_state->old_origin));

    if (delta_bits_check(bits, U_SOUND, &entity_state->delta_bits, Q2P_ESD_SOUND)) {
        uint16_t sound_word;
        READ_CHECKED(client_read, io_arg, sound_word, u16);
        entity_state->sound = sound_word & 0x3fff;
        if (delta_bits_check(sound_word, SOUND_FLAG_VOLUME, &entity_state->delta_bits, Q2P_ESD_LOOP_VOLUME)) {
            MAYBE_UNUSED uint8_t loop_volume;
            READ_CHECKED(client_read, io_arg, loop_volume, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
            entity_state->loop_volume = loop_volume;
#endif
        }
        if (delta_bits_check(sound_word, SOUND_FLAG_ATTENUATION, &entity_state->delta_bits, Q2P_ESD_LOOP_ATTENUATION)) {
            MAYBE_UNUSED uint8_t loop_attenuation;
            READ_CHECKED(client_read, io_arg, loop_attenuation, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
            entity_state->loop_attenuation = loop_attenuation;
#endif
        }
    }

    if (delta_bits_check(bits, U_EVENT, &entity_state->delta_bits, Q2P_ESD_EVENT))
        READ_CHECKED(client_read, io_arg, entity_state->event, u8);

    if (delta_bits_check(bits, U_SOLID, &entity_state->delta_bits, Q2P_ESD_SOLID))
        READ_CHECKED(client_read, io_arg, entity_state->solid, u32);

    if (delta_bits_check(bits, U_MOREFX32, &entity_state->delta_bits, Q2P_ESD_EFFECTS_MORE)) {
        MAYBE_UNUSED uint32_t effects_more = 0;
        if ((bits & U_MOREFX32) == U_MOREFX32)
            READ_CHECKED(client_read, io_arg, effects_more, u32);
        else if (bits & U_MOREFX16)
            READ_CHECKED(client_read, io_arg, effects_more, u16);
        else if (bits & U_MOREFX8)
            READ_CHECKED(client_read, io_arg, effects_more, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        entity_state->effects_more = effects_more;
#endif
    }

    if (delta_bits_check(bits, U_ALPHA, &entity_state->delta_bits, Q2P_ESD_ALPHA)) {
        MAYBE_UNUSED uint8_t alpha;
        READ_CHECKED(client_read, io_arg, alpha, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        entity_state->alpha = alpha;
#endif
    }

    if (delta_bits_check(bits, U_SCALE, &entity_state->delta_bits, Q2P_ESD_SCALE)) {
        MAYBE_UNUSED uint8_t scale;
        READ_CHECKED(client_read, io_arg, scale, u8);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        entity_state->scale = scale;
#endif
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_read_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                    q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    uint64_t bits;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &spawnbaseline->entnum));

    q2proto_debug_shownet_entity_delta_bits(io_arg, "   baseline:", spawnbaseline->entnum, bits);

    return q2repro_client_read_entity_delta(context, io_arg, bits, &spawnbaseline->delta_state);
}

static q2proto_error_t q2repro_client_read_playerstate(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                       uint8_t extraflags, q2proto_svc_playerstate_t *playerstate)
{
    uint16_t flags;
    READ_CHECKED(client_read, io_arg, flags, u16);

#if Q2PROTO_SHOWNET
    if (q2protodbg_shownet_check(io_arg, 2) && flags) {
        char buf[1024], buf2[1024];
        debug_player_delta_bits_to_str(buf, sizeof(buf), flags);
        q2proto_debug_common_player_delta_extrabits_to_str(buf2, sizeof(buf2), extraflags);
        SHOWNET(io_arg, 2, -2, "   %s + %s", buf, buf2);
    }
#endif

    //
    // parse the pmove_state_t
    //
    if (delta_bits_check(flags, PS_M_TYPE, &playerstate->delta_bits, Q2P_PSD_PM_TYPE))
        READ_CHECKED(client_read, io_arg, playerstate->pm_type, u8);

    if (flags & PS_M_ORIGIN) {
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &playerstate->pm_origin.read.value.values, 0);
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &playerstate->pm_origin.read.value.values, 1);
        playerstate->pm_origin.read.value.delta_bits |= BIT(0) | BIT(1);
    }
    if (extraflags & EPS_M_ORIGIN2) {
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &playerstate->pm_origin.read.value.values, 2);
        playerstate->pm_origin.read.value.delta_bits |= BIT(2);
    }

    if (flags & PS_M_VELOCITY) {
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &playerstate->pm_velocity.read.value.values, 0);
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &playerstate->pm_velocity.read.value.values, 1);
        playerstate->pm_velocity.read.value.delta_bits |= BIT(0) | BIT(1);
    }
    if (extraflags & EPS_M_VELOCITY2) {
        READ_CHECKED_VAR_COORDS_COMP_FLOAT(client_read, io_arg, &playerstate->pm_velocity.read.value.values, 2);
        playerstate->pm_velocity.read.value.delta_bits |= BIT(2);
    }

    if (delta_bits_check(flags, PS_M_TIME, &playerstate->delta_bits, Q2P_PSD_PM_TIME))
        READ_CHECKED(client_read, io_arg, playerstate->pm_time, u16);

    if (delta_bits_check(flags, PS_M_FLAGS, &playerstate->delta_bits, Q2P_PSD_PM_FLAGS))
        READ_CHECKED(client_read, io_arg, playerstate->pm_flags, u16);

    if (delta_bits_check(flags, PS_M_GRAVITY, &playerstate->delta_bits, Q2P_PSD_PM_GRAVITY))
        READ_CHECKED(client_read, io_arg, playerstate->pm_gravity, i16);

    if (delta_bits_check(flags, PS_M_DELTA_ANGLES, &playerstate->delta_bits, Q2P_PSD_PM_DELTA_ANGLES))
        CHECKED(client_read, io_arg, read_var_angles16(io_arg, &playerstate->pm_delta_angles));

    //
    // parse the rest of the player_state_t
    //
    if (delta_bits_check(flags, PS_VIEWOFFSET, &playerstate->delta_bits, Q2P_PSD_VIEWOFFSET))
        CHECKED(client_read, io_arg, read_viewoffsets_q2repro(io_arg, &playerstate->viewoffset));

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
        CHECKED(client_read, io_arg, read_kickangles_q2repro(io_arg, &playerstate->kick_angles));

    if (delta_bits_check(flags, PS_WEAPONINDEX, &playerstate->delta_bits, Q2P_PSD_GUNINDEX)) {
        uint16_t gun_index_and_skin;
        READ_CHECKED(client_read, io_arg, gun_index_and_skin, u16);
        playerstate->gunindex = gun_index_and_skin & Q2PRO_GUNINDEX_MASK;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        playerstate->gunskin = gun_index_and_skin >> Q2PRO_GUNINDEX_BITS;
#endif
    }

    if (delta_bits_check(flags, PS_WEAPONFRAME, &playerstate->delta_bits, Q2P_PSD_GUNFRAME))
        READ_CHECKED(client_read, io_arg, playerstate->gunframe, u16);
    if (extraflags & EPS_GUNOFFSET) {
        CHECKED(client_read, io_arg, read_short_gunoffset(io_arg, &playerstate->gunoffset.values));
        playerstate->gunoffset.delta_bits = BIT(0) | BIT(1) | BIT(2);
    }
    if (extraflags & EPS_GUNANGLES) {
        CHECKED(client_read, io_arg, read_short_gunangles(io_arg, &playerstate->gunangles.values));
        playerstate->gunangles.delta_bits = BIT(0) | BIT(1) | BIT(2);
    }

    if (flags & PS_BLEND) {
        q2proto_color_delta_t damage_blend = {0};
        CHECKED(client_read, io_arg, client_read_q2pro_extv2_blends(io_arg, &playerstate->blend, &damage_blend));
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
        memcpy(&playerstate->damage_blend, &damage_blend, sizeof(damage_blend));
#endif
    }

    if (delta_bits_check(flags, PS_FOV, &playerstate->delta_bits, Q2P_PSD_FOV))
        READ_CHECKED(client_read, io_arg, playerstate->fov, u8);

    if (delta_bits_check(flags, PS_RDFLAGS, &playerstate->delta_bits, Q2P_PSD_RDFLAGS))
        READ_CHECKED(client_read, io_arg, playerstate->rdflags, u8);

    // parse stats
    if (extraflags & EPS_STATS) {
        int numstats = 64;
        READ_CHECKED(client_read, io_arg, playerstate->statbits, u64);
        for (int i = 0; i < numstats; i++)
            if (playerstate->statbits & BIT_ULL(i))
                READ_CHECKED(client_read, io_arg, playerstate->stats[i], i16);
    }

#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
    if (delta_bits_check(extraflags, EPS_GUNRATE, &playerstate->delta_bits, Q2P_PSD_GUNRATE))
        READ_CHECKED(client_read, io_arg, playerstate->gunrate, u8);

    if (delta_bits_check(flags, PS_RR_VIEWHEIGHT, &playerstate->delta_bits, Q2P_PSD_PM_VIEWHEIGHT))
        READ_CHECKED(client_read, io_arg, playerstate->pm_viewheight, i8);
#endif

    if (delta_bits_check(extraflags, EPS_CLIENTNUM, &playerstate->delta_bits, Q2P_PSD_CLIENTNUM))
        READ_CHECKED(client_read, io_arg, playerstate->clientnum, i16);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_q2repro_client_read_muzzleflash3(uintptr_t io_arg, q2proto_svc_muzzleflash_t *muzzleflash)
{
    READ_CHECKED(client_read, io_arg, muzzleflash->entity, i16);
    READ_CHECKED(client_read, io_arg, muzzleflash->weapon, u16);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_read_frame(q2proto_clientcontext_t *context, uintptr_t io_arg,
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

    READ_CHECKED(client_read, io_arg, frame->q2pro_frame_flags, u8);
    frame->q2pro_frame_flags &= 0x0F;
    uint8_t extraflags;
    READ_CHECKED(client_read, io_arg, extraflags, u8);

    // read areabits
    READ_CHECKED(client_read, io_arg, frame->areabits_len, u8);
    READ_CHECKED(client_read, io_arg, frame->areabits, raw, frame->areabits_len, NULL);

    // read playerinfo
    SHOWNET(io_arg, 2, 0, "playerinfo");
    CHECKED(client_read, io_arg, q2repro_client_read_playerstate(context, io_arg, extraflags, &frame->playerstate));

    // read packet entities
    context->client_read = q2repro_client_read_delta_entities;
    SHOWNET(io_arg, 2, 0, "packetentities");

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_q2repro_client_read_damage(uintptr_t io_arg, q2proto_svc_damage_t *damage)
{
    READ_CHECKED(client_read, io_arg, damage->count, u8);
    for (unsigned int i = 0; i < damage->count; i++) {
        uint8_t encoded_damage;
        READ_CHECKED(client_read, io_arg, encoded_damage, u8);
        q2proto_vec3_t dir;
        CHECKED(client_read, io_arg, q2proto_common_client_read_packed_direction(io_arg, dir));

        if (i >= Q2PROTO_MAX_DAMAGE_INDICATORS)
            continue;

        damage->damage[i].damage = encoded_damage & 0x1f;
        damage->damage[i].health = (encoded_damage & 0x20) != 0;
        damage->damage[i].armor = (encoded_damage & 0x40) != 0;
        damage->damage[i].shield = (encoded_damage & 0x80) != 0;
        memcpy(&damage->damage[i].direction, &dir, sizeof(q2proto_vec3_t));
    }

    return Q2P_ERR_SUCCESS;
}

static inline bool fog_color_bits_check(unsigned int bits, unsigned int check, uint8_t *delta_new, uint8_t new_bits)
{
    if (bits & check) {
        *delta_new |= new_bits;
        return true;
    }
    return false;
}

q2proto_error_t q2proto_q2repro_client_read_fog(uintptr_t io_arg, q2proto_svc_fog_t *fog)
{
    unsigned int bits;
    READ_CHECKED(client_read, io_arg, bits, u8);
    if (bits & FOG_RR_BIT_MORE_BITS) {
        unsigned int bits2;
        READ_CHECKED(client_read, io_arg, bits2, u8);
        bits |= bits2 << 8;
    }

    if (delta_bits_check(bits, FOG_RR_BIT_DENSITY, &fog->flags, Q2P_FOG_DENSITY_SKYFACTOR)) {
        float density;
        READ_CHECKED(client_read, io_arg, density, float);
        q2proto_var_fraction_set_float(&fog->global.density, density);
        uint8_t skyfactor;
        READ_CHECKED(client_read, io_arg, skyfactor, u8);
        q2proto_var_fraction_set_byte(&fog->global.skyfactor, skyfactor);
    }
    if (fog_color_bits_check(bits, FOG_RR_BIT_R, &fog->global.color.delta_bits, BIT(0)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->global.color.values, 0);
    if (fog_color_bits_check(bits, FOG_RR_BIT_G, &fog->global.color.delta_bits, BIT(1)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->global.color.values, 1);
    if (fog_color_bits_check(bits, FOG_RR_BIT_B, &fog->global.color.delta_bits, BIT(2)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->global.color.values, 2);
    if (delta_bits_check(bits, FOG_RR_BIT_TIME, &fog->flags, Q2P_FOG_TIME))
        READ_CHECKED(client_read, io_arg, fog->global.time, u16);

    if (delta_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_FALLOFF, &fog->flags, Q2P_HEIGHTFOG_FALLOFF)) {
        float falloff;
        READ_CHECKED(client_read, io_arg, falloff, float);
        q2proto_var_fraction_set_float(&fog->height.falloff, falloff);
    }
    if (delta_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_DENSITY, &fog->flags, Q2P_HEIGHTFOG_DENSITY)) {
        float density;
        READ_CHECKED(client_read, io_arg, density, float);
        q2proto_var_fraction_set_float(&fog->height.density, density);
    }

    if (fog_color_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_START_R, &fog->height.start_color.delta_bits, BIT(0)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->height.start_color.values, 0);
    if (fog_color_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_START_G, &fog->height.start_color.delta_bits, BIT(1)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->height.start_color.values, 1);
    if (fog_color_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_START_B, &fog->height.start_color.delta_bits, BIT(2)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->height.start_color.values, 2);
    if (delta_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_START_DIST, &fog->flags, Q2P_HEIGHTFOG_START_DIST)) {
        int32_t dist;
        READ_CHECKED(client_read, io_arg, dist, i32);
        q2proto_var_coord_set_int_unscaled(&fog->height.start_dist, dist);
    }

    if (fog_color_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_END_R, &fog->height.end_color.delta_bits, BIT(0)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->height.end_color.values, 0);
    if (fog_color_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_END_G, &fog->height.end_color.delta_bits, BIT(1)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->height.end_color.values, 1);
    if (fog_color_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_END_B, &fog->height.end_color.delta_bits, BIT(2)))
        READ_CHECKED_VAR_COLOR_COMP(client_read, io_arg, &fog->height.end_color.values, 2);
    if (delta_bits_check(bits, FOG_RR_BIT_HEIGHTFOG_END_DIST, &fog->flags, Q2P_HEIGHTFOG_END_DIST)) {
        int32_t dist;
        READ_CHECKED(client_read, io_arg, dist, i32);
        q2proto_var_coord_set_int_unscaled(&fog->height.end_dist, dist);
    }

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_q2repro_client_read_poi(uintptr_t io_arg, q2proto_svc_poi_t *poi)
{
    READ_CHECKED(client_read, io_arg, poi->key, u16);
    READ_CHECKED(client_read, io_arg, poi->time, u16);
    CHECKED(client_read, io_arg, read_float_coord(io_arg, poi->pos));
    READ_CHECKED(client_read, io_arg, poi->image, u16);
    READ_CHECKED(client_read, io_arg, poi->color, u8);
    READ_CHECKED(client_read, io_arg, poi->flags, u8);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_q2repro_client_read_help_path(uintptr_t io_arg, q2proto_svc_help_path_t *help_path)
{
    uint8_t start;
    READ_CHECKED(client_read, io_arg, start, u8);
    help_path->start = start != 0;

    CHECKED(client_read, io_arg, read_float_coord(io_arg, help_path->pos));
    CHECKED(client_read, io_arg, q2proto_common_client_read_packed_direction(io_arg, help_path->dir));

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_q2repro_client_read_achievement(uintptr_t io_arg, q2proto_svc_achievement_t *achievement)
{
    READ_CHECKED(client_read, io_arg, achievement->id, string);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_read_zdownload(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                     q2proto_svc_download_t *download)
{
#if Q2PROTO_COMPRESSION_DEFLATE
    READ_CHECKED(client_read, io_arg, download->size, i16);
    READ_CHECKED(client_read, io_arg, download->percent, u8);

    // FIXME: should end inflate in case of an error...
    if (!context->has_zdownload_inflate_io_arg) {
        CHECKED(client_read, io_arg,
                q2protoio_inflate_begin(io_arg, Q2P_INFL_DEFL_RAW, &context->zdownload_inflate_io_arg));
        context->has_zdownload_inflate_io_arg = true;
    }
    CHECKED(client_read, io_arg, q2protoio_inflate_data(io_arg, context->zdownload_inflate_io_arg, download->size));
    size_t uncompressed_len = 0;
    READ_CHECKED(client_read, context->zdownload_inflate_io_arg, download->data, raw, SIZE_MAX, &uncompressed_len);
    bool stream_end = false;
    CHECKED(client_read, io_arg, q2protoio_inflate_stream_ended(context->zdownload_inflate_io_arg, &stream_end));
    if (stream_end) {
        CHECKED_IO(client_read, io_arg, q2protoio_inflate_end(context->zdownload_inflate_io_arg), "finishing inflate");
        context->has_zdownload_inflate_io_arg = false;
    }
    download->size = uncompressed_len;
    return Q2P_ERR_SUCCESS;
#else
    return Q2P_ERR_DEFLATE_NOT_SUPPORTED;
#endif
}

static q2proto_error_t q2repro_client_read_streamed_configstring(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                                 q2proto_svc_message_t *svc_message)
{
    unsigned int max_configstrings = max_configstrings_for_game(context->features.server_game_api);
    uint16_t index;
    READ_CHECKED(client_read, io_arg, index, u16);
    if (index == max_configstrings)
        return Q2P_ERR_NO_MORE_INPUT;

    svc_message->type = Q2P_SVC_CONFIGSTRING;
    svc_message->configstring.index = index;
    READ_CHECKED(client_read, io_arg, svc_message->configstring.value, string);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_read_streamed_baseline(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                                             q2proto_svc_message_t *svc_message)
{
    uint64_t bits;
    uint16_t entnum;
    CHECKED(client_read, io_arg, q2proto_common_client_read_entity_bits(io_arg, &bits, &entnum));

    if (entnum == 0)
        return Q2P_ERR_NO_MORE_INPUT;

    q2proto_debug_shownet_entity_delta_bits(io_arg, "   baseline:", entnum, bits);

    svc_message->type = Q2P_SVC_SPAWNBASELINE;
    memset(&svc_message->spawnbaseline, 0, sizeof(svc_message->spawnbaseline));
    svc_message->spawnbaseline.entnum = entnum;
    return q2repro_client_read_entity_delta(context, io_arg, bits, &svc_message->spawnbaseline.delta_state);
}

static q2proto_error_t q2repro_client_read_gamestate_baseline(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                              q2proto_svc_message_t *svc_message)
{
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    q2proto_error_t result = q2repro_client_read_streamed_baseline(context, io_arg, svc_message);

    if (result == Q2P_ERR_NO_MORE_INPUT) {
        context->client_read = q2repro_client_read;
        return q2repro_client_read(context, raw_io_arg, svc_message);
    }

    return result;
}

static q2proto_error_t q2repro_client_read_gamestate_configstring(q2proto_clientcontext_t *context,
                                                                  uintptr_t raw_io_arg,
                                                                  q2proto_svc_message_t *svc_message)
{
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    q2proto_error_t result = q2repro_client_read_streamed_configstring(context, io_arg, svc_message);
    if (result == Q2P_ERR_NO_MORE_INPUT) {
        context->client_read = q2repro_client_read_gamestate_baseline;
        return q2repro_client_read_gamestate_baseline(context, raw_io_arg, svc_message);
    }

    return result;
}

static q2proto_error_t q2repro_client_read_begin_gamestate(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                           q2proto_svc_message_t *svc_message)
{
    context->client_read = q2repro_client_read_gamestate_configstring;
    return q2repro_client_read_gamestate_configstring(context, raw_io_arg, svc_message);
}

static q2proto_error_t q2repro_client_read_configstream(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                        q2proto_svc_message_t *svc_message)
{
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    q2proto_error_t result = q2repro_client_read_streamed_configstring(context, io_arg, svc_message);
    if (result == Q2P_ERR_NO_MORE_INPUT) {
        context->client_read = q2repro_client_read;
        return q2repro_client_read(context, raw_io_arg, svc_message);
    }

    return result;
}

static q2proto_error_t q2repro_client_read_begin_configstream(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                              q2proto_svc_message_t *svc_message)
{
    context->client_read = q2repro_client_read_configstream;
    return q2repro_client_read_configstream(context, raw_io_arg, svc_message);
}

static q2proto_error_t q2repro_client_read_baselinestream(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                          q2proto_svc_message_t *svc_message)
{
    uintptr_t io_arg = context->has_inflate_io_arg ? context->inflate_io_arg : raw_io_arg;

    q2proto_error_t result = q2repro_client_read_streamed_baseline(context, io_arg, svc_message);
    if (result == Q2P_ERR_NO_MORE_INPUT) {
        context->client_read = q2repro_client_read;
        return q2repro_client_read(context, raw_io_arg, svc_message);
    }

    return result;
}

static q2proto_error_t q2repro_client_read_begin_baselinestream(q2proto_clientcontext_t *context, uintptr_t raw_io_arg,
                                                                q2proto_svc_message_t *svc_message)
{
    context->client_read = q2repro_client_read_baselinestream;
    return q2repro_client_read_baselinestream(context, raw_io_arg, svc_message);
}


//
// CLIENT: SEND MESSAGES TO SERVER
//

static q2proto_error_t q2repro_client_write_move(uintptr_t io_arg, const q2proto_clc_move_t *move);
static q2proto_error_t q2repro_client_write_batch_move(uintptr_t io_arg, const q2proto_clc_batch_move_t *move);
static q2proto_error_t q2repro_client_write_userinfo_delta(uintptr_t io_arg,
                                                           const q2proto_clc_userinfo_delta_t *userinfo_delta);

static q2proto_error_t q2repro_client_write(q2proto_clientcontext_t *context, uintptr_t io_arg,
                                            const q2proto_clc_message_t *clc_message)
{
    switch (clc_message->type) {
    case Q2P_CLC_NOP:
        return q2proto_common_client_write_nop(io_arg);

    case Q2P_CLC_MOVE:
        return q2repro_client_write_move(io_arg, &clc_message->move);

    case Q2P_CLC_BATCH_MOVE:
        return q2repro_client_write_batch_move(io_arg, &clc_message->batch_move);

    case Q2P_CLC_USERINFO:
        return q2proto_common_client_write_userinfo(io_arg, &clc_message->userinfo);

    case Q2P_CLC_STRINGCMD:
        return q2proto_common_client_write_stringcmd(io_arg, &clc_message->stringcmd);

    case Q2P_CLC_SETTING:
        return r1q2_client_write_setting(io_arg, &clc_message->setting);

    case Q2P_CLC_USERINFO_DELTA:
        return q2repro_client_write_userinfo_delta(io_arg, &clc_message->userinfo_delta);

    default:
        break;
    }

    return Q2P_ERR_BAD_COMMAND;
}

static q2proto_error_t q2repro_client_write_move_delta(uintptr_t io_arg, const q2proto_clc_move_delta_t *move_delta)
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
        return Q2P_ERR_BAD_DATA;
    if (move_delta->delta_bits & Q2P_CMD_BUTTONS)
        bits |= CM_BUTTONS;
    if (move_delta->delta_bits & Q2P_CMD_IMPULSE)
        bits |= CM_IMPULSE;

    WRITE_CHECKED(client_write, io_arg, u8, bits);

    // FIXME?: Write move values as float?
    int16_t short_move[3];
    int16_t short_angles[3];
    q2proto_var_coords_get_short_unscaled(&move_delta->move, short_move);
    q2proto_var_angles_get_short(&move_delta->angles, short_angles);

    if (bits & CM_ANGLE1)
        WRITE_CHECKED(client_write, io_arg, i16, short_angles[0]);
    if (bits & CM_ANGLE2)
        WRITE_CHECKED(client_write, io_arg, i16, short_angles[1]);
    if (bits & CM_ANGLE3)
        WRITE_CHECKED(client_write, io_arg, i16, short_angles[2]);

    if (bits & CM_FORWARD)
        WRITE_CHECKED(client_write, io_arg, i16, short_move[0]);
    if (bits & CM_SIDE)
        WRITE_CHECKED(client_write, io_arg, i16, short_move[1]);

    if (bits & CM_BUTTONS)
        WRITE_CHECKED(client_write, io_arg, u8, move_delta->buttons);
    if (bits & CM_IMPULSE)
        WRITE_CHECKED(client_write, io_arg, u8, move_delta->impulse);

    WRITE_CHECKED(client_write, io_arg, u8, move_delta->msec);
    WRITE_CHECKED(client_write, io_arg, u8, move_delta->lightlevel);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_write_move(uintptr_t io_arg, const q2proto_clc_move_t *move)
{
    WRITE_CHECKED(client_write, io_arg, u8, clc_move);
    WRITE_CHECKED(client_write, io_arg, i32, move->lastframe);
    for (int i = 0; i < 3; i++) {
        CHECKED(client_write, io_arg, q2repro_client_write_move_delta(io_arg, &move->moves[i]));
    }
    return Q2P_ERR_SUCCESS;
}

#define CHECKED_BITWRITER_WRITE(BITWRITER, VALUE, NUMBITS) \
    CHECKED(client_write, (BITWRITER)->io_arg, bitwriter_write(bitwriter, (VALUE), (NUMBITS)))

static q2proto_error_t q2repro_client_write_move_delta_bits(bitwriter_t *bitwriter,
                                                            const q2proto_clc_move_delta_t *move_delta,
                                                            q2proto_clc_move_delta_t *base_move)
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
        return Q2P_ERR_BAD_DATA;
    if (move_delta->delta_bits & Q2P_CMD_BUTTONS)
        bits |= CM_BUTTONS;
    if (move_delta->msec != base_move->msec)
        bits |= CM_IMPULSE;

    if (!bits) {
        CHECKED_BITWRITER_WRITE(bitwriter, 0, 1);
        return Q2P_ERR_SUCCESS;
    }

    CHECKED_BITWRITER_WRITE(bitwriter, 1, 1);
    CHECKED_BITWRITER_WRITE(bitwriter, bits, 8);

    // FIXME?: Write move values as float?
    int16_t short_move[3];
    int16_t short_angles[3];
    int16_t prev_short_angles[3] = {0};
    q2proto_var_coords_get_short_unscaled(&move_delta->move, short_move);
    q2proto_var_angles_get_short(&move_delta->angles, short_angles);
    q2proto_var_angles_get_short(&base_move->angles, prev_short_angles);

    if (bits & CM_ANGLE1) {
        int delta = short_angles[0] - prev_short_angles[0];
        if (delta >= -128 && delta <= 127) {
            CHECKED_BITWRITER_WRITE(bitwriter, 1, 1);
            CHECKED_BITWRITER_WRITE(bitwriter, delta, -8);
        } else {
            CHECKED_BITWRITER_WRITE(bitwriter, 0, 1);
            CHECKED_BITWRITER_WRITE(bitwriter, short_angles[0], -16);
        }
        q2proto_var_angles_set_short_comp(&base_move->angles, 0, short_angles[0]);
    }
    if (bits & CM_ANGLE2) {
        int delta = short_angles[1] - prev_short_angles[1];
        if (delta >= -128 && delta <= 127) {
            CHECKED_BITWRITER_WRITE(bitwriter, 1, 1);
            CHECKED_BITWRITER_WRITE(bitwriter, delta, -8);
        } else {
            CHECKED_BITWRITER_WRITE(bitwriter, 0, 1);
            CHECKED_BITWRITER_WRITE(bitwriter, short_angles[1], -16);
        }
        q2proto_var_angles_set_short_comp(&base_move->angles, 1, short_angles[1]);
    }
    if (bits & CM_ANGLE3) {
        CHECKED_BITWRITER_WRITE(bitwriter, short_angles[2], -16);
        q2proto_var_angles_set_short_comp(&base_move->angles, 2, short_angles[2]);
    }

    if (bits & CM_FORWARD) {
        CHECKED_BITWRITER_WRITE(bitwriter, short_move[0], -10);
        q2proto_var_coords_set_float_comp(&base_move->move, 0, short_move[0]);
    }
    if (bits & CM_SIDE) {
        CHECKED_BITWRITER_WRITE(bitwriter, short_move[1], -10);
        q2proto_var_coords_set_float_comp(&base_move->move, 1, short_move[1]);
    }

    if (bits & CM_BUTTONS) {
        CHECKED_BITWRITER_WRITE(bitwriter, move_delta->buttons, 8);
        base_move->buttons = move_delta->buttons;
    }
    if (bits & CM_IMPULSE) {
        CHECKED_BITWRITER_WRITE(bitwriter, move_delta->msec, 8);
        base_move->msec = move_delta->msec;
    }

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_write_batch_move_frame(bitwriter_t *bitwriter,
                                                             const q2proto_clc_batch_move_frame_t *move_frame,
                                                             q2proto_clc_move_delta_t *base_move)
{
    CHECKED_BITWRITER_WRITE(bitwriter, move_frame->num_cmds, 5);

    for (int i = 0; i < move_frame->num_cmds; i++) {
        const q2proto_clc_move_delta_t *current_move = &move_frame->moves[i];
        CHECKED(client_write, bitwriter->io_arg,
                q2repro_client_write_move_delta_bits(bitwriter, current_move, base_move));
    }
    return Q2P_ERR_SUCCESS;
}

#undef CHECKED_BITWRITER_WRITE

static q2proto_error_t q2repro_client_write_batch_move(uintptr_t io_arg, const q2proto_clc_batch_move_t *batch_move)
{
    uint8_t cmd;
    if (batch_move->lastframe == -1)
        cmd = clc_q2pro_move_nodelta;
    else
        cmd = clc_q2pro_move_batched;

    WRITE_CHECKED(client_write, io_arg, u8, cmd);
    if (batch_move->lastframe != -1)
        WRITE_CHECKED(client_write, io_arg, i32, batch_move->lastframe);

    WRITE_CHECKED(client_write, io_arg, u8, batch_move->num_dups);

    // send lightlevel, take it from last command
    const q2proto_clc_batch_move_frame_t *last_move_frame = &batch_move->batch_frames[batch_move->num_dups];
    uint8_t lightlevel = last_move_frame->moves[last_move_frame->num_cmds - 1].lightlevel;
    WRITE_CHECKED(client_write, io_arg, u8, lightlevel);

    q2proto_clc_move_delta_t base_move;
    memset(&base_move, 0, sizeof(base_move));

    bitwriter_t bitwriter;
    bitwriter_init(&bitwriter, io_arg);
    for (int i = 0; i < batch_move->num_dups + 1; i++) {
        const q2proto_clc_batch_move_frame_t *move_frame = &batch_move->batch_frames[i];
        CHECKED(client_write, io_arg, q2repro_client_write_batch_move_frame(&bitwriter, move_frame, &base_move));
    }
    CHECKED(client_write, io_arg, bitwriter_flush(&bitwriter));
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_client_write_userinfo_delta(uintptr_t io_arg,
                                                           const q2proto_clc_userinfo_delta_t *userinfo_delta)
{
    WRITE_CHECKED(client_write, io_arg, u8, clc_q2pro_userinfo_delta);
    WRITE_CHECKED(client_write, io_arg, string, &userinfo_delta->name);
    WRITE_CHECKED(client_write, io_arg, string, &userinfo_delta->value);
    return Q2P_ERR_SUCCESS;
}

static uint32_t q2repro_pack_solid(q2proto_clientcontext_t *context, const q2proto_vec3_t mins,
                                   const q2proto_vec3_t maxs)
{
    return q2proto_pack_solid_32_q2pro_v2(mins, maxs);
}

static void q2repro_unpack_solid(q2proto_clientcontext_t *context, uint32_t solid, q2proto_vec3_t mins,
                                 q2proto_vec3_t maxs)
{
    q2proto_unpack_solid_32_q2pro_v2(solid, mins, maxs);
}

//
// SERVER: INITIALIZATION
//

static q2proto_error_t q2repro_server_fill_serverdata(q2proto_servercontext_t *context,
                                                      q2proto_svc_serverdata_t *serverdata);
static void q2repro_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_entity_state_t *from,
                                                   const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                   q2proto_entity_state_delta_t *delta);
static void q2repro_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_player_state_t *from,
                                                   const q2proto_packed_player_state_t *to,
                                                   q2proto_svc_playerstate_t *delta);
static q2proto_error_t q2repro_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
                                            const q2proto_svc_message_t *svc_message);
static q2proto_error_t q2repro_server_write_gamestate(q2proto_servercontext_t *context,
                                                      q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                      const q2proto_gamestate_t *gamestate);
static q2proto_error_t q2repro_server_read(q2proto_servercontext_t *context, uintptr_t io_arg,
                                           q2proto_clc_message_t *clc_message);

static q2proto_error_t q2repro_download_begin(q2proto_servercontext_t *context, q2proto_server_download_state_t *state,
                                              q2proto_download_compress_t compress,
                                              q2protoio_deflate_args_t *deflate_args);
static q2proto_error_t q2repro_download_data(q2proto_server_download_state_t *state, const uint8_t **data,
                                             size_t *remaining, size_t packet_remaining,
                                             q2proto_svc_download_t *svc_download);

static const struct q2proto_download_funcs_s q2repro_download_funcs = {.begin = q2repro_download_begin,
                                                                       .data = q2repro_download_data,
                                                                       .finish = q2proto_download_common_finish,
                                                                       .abort = q2proto_download_common_abort};

q2proto_error_t q2proto_q2repro_init_servercontext(q2proto_servercontext_t *context,
                                                   const q2proto_connect_t *connect_info)
{
    context->protocol_version = connect_info->version;
    context->zpacket_cmd = svc_q2repro_zpacket;
    context->features.enable_deflate = connect_info->has_zlib;
    context->features.download_compress_raw = true;
    context->features.has_beam_old_origin_fix = true;
    context->features.playerstate_clientnum = true;

    context->fill_serverdata = q2repro_server_fill_serverdata;
    context->make_entity_state_delta = q2repro_server_make_entity_state_delta;
    context->make_player_state_delta = q2repro_server_make_player_state_delta;
    context->server_write = q2repro_server_write;
    context->server_write_gamestate = q2repro_server_write_gamestate;
    context->server_read = q2repro_server_read;
    context->download_funcs = &q2repro_download_funcs;
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_fill_serverdata(q2proto_servercontext_t *context,
                                                      q2proto_svc_serverdata_t *serverdata)
{
    serverdata->protocol = PROTOCOL_Q2REPRO;
    serverdata->protocol_version = context->protocol_version;
    serverdata->q2pro.extensions = context->server_info->game_api >= Q2PROTO_GAME_Q2PRO_EXTENDED;
    serverdata->q2pro.extensions_v2 = context->server_info->game_api >= Q2PROTO_GAME_Q2PRO_EXTENDED_V2;
    serverdata->q2repro.game3_compat = context->server_info->game_api != Q2PROTO_GAME_RERELEASE;
    return Q2P_ERR_SUCCESS;
}

static void q2repro_server_make_entity_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_entity_state_t *from,
                                                   const q2proto_packed_entity_state_t *to, bool write_old_origin,
                                                   q2proto_entity_state_delta_t *delta)
{
    memset(delta, 0, sizeof(*delta));

    if (!from)
        from = &q2proto_null_packed_entity_state;

    for (int c = 0; c < 3; c++) {
        q2proto_var_coords_set_float_comp(&delta->origin.write.prev, c, _q2proto_valenc_bits2float(from->origin[c]));
        q2proto_var_coords_set_float_comp(&delta->origin.write.current, c, _q2proto_valenc_bits2float(to->origin[c]));
    }

    if (to->angles[0] != from->angles[0]) {
        delta->angle.delta_bits |= BIT(0);
        q2proto_var_angles_set_short_comp(&delta->angle.values, 0, to->angles[0]);
    }
    if (to->angles[1] != from->angles[1]) {
        delta->angle.delta_bits |= BIT(1);
        q2proto_var_angles_set_short_comp(&delta->angle.values, 1, to->angles[1]);
    }
    if (to->angles[2] != from->angles[2]) {
        delta->angle.delta_bits |= BIT(2);
        q2proto_var_angles_set_short_comp(&delta->angle.values, 2, to->angles[2]);
    }

    if (write_old_origin) {
        delta->delta_bits |= Q2P_ESD_OLD_ORIGIN;
        q2proto_var_coords_set_float_comp(&delta->old_origin, 0, _q2proto_valenc_bits2float(to->old_origin[0]));
        q2proto_var_coords_set_float_comp(&delta->old_origin, 1, _q2proto_valenc_bits2float(to->old_origin[1]));
        q2proto_var_coords_set_float_comp(&delta->old_origin, 2, _q2proto_valenc_bits2float(to->old_origin[2]));
    }

    if (to->skinnum != from->skinnum) {
        delta->delta_bits |= Q2P_ESD_SKINNUM;
        delta->skinnum = to->skinnum;
    }

    if (to->frame != from->frame) {
        delta->delta_bits |= Q2P_ESD_FRAME;
        delta->frame = to->frame;
    }

    if (to->effects != from->effects) {
        if ((uint32_t)to->effects != (uint32_t)from->effects)
            delta->delta_bits |= Q2P_ESD_EFFECTS;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        if ((to->effects >> 32) != (from->effects >> 32))
            delta->delta_bits |= Q2P_ESD_EFFECTS_MORE;
#endif
        if (delta->delta_bits & (Q2P_ESD_EFFECTS | Q2P_ESD_EFFECTS_MORE)) {
            delta->effects = to->effects;
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
            delta->effects_more = to->effects >> 32;
#endif
        }
    }

    if (to->renderfx != from->renderfx) {
        delta->delta_bits |= Q2P_ESD_RENDERFX;
        delta->renderfx = to->renderfx;
    }

    if (to->solid != from->solid) {
        delta->delta_bits |= Q2P_ESD_SOLID;
        delta->solid = to->solid;
    }

    // event is not delta compressed, just 0 compressed
    if (to->event) {
        delta->delta_bits |= Q2P_ESD_EVENT;
        delta->event = to->event;
    }

    if (to->modelindex != from->modelindex) {
        delta->delta_bits |= Q2P_ESD_MODELINDEX;
        delta->modelindex = to->modelindex;
    }
    if (to->modelindex2 != from->modelindex2) {
        delta->delta_bits |= Q2P_ESD_MODELINDEX2;
        delta->modelindex2 = to->modelindex2;
    }
    if (to->modelindex3 != from->modelindex3) {
        delta->delta_bits |= Q2P_ESD_MODELINDEX3;
        delta->modelindex3 = to->modelindex3;
    }
    if (to->modelindex4 != from->modelindex4) {
        delta->delta_bits |= Q2P_ESD_MODELINDEX4;
        delta->modelindex4 = to->modelindex4;
    }

    if (to->sound != from->sound) {
        delta->delta_bits |= Q2P_ESD_SOUND;
        delta->sound = to->sound;
    }

#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    if (to->loop_volume != from->loop_volume) {
        delta->delta_bits |= Q2P_ESD_LOOP_VOLUME;
        delta->loop_volume = to->loop_volume;
    }
    if (to->loop_attenuation != from->loop_attenuation) {
        delta->delta_bits |= Q2P_ESD_LOOP_ATTENUATION;
        delta->loop_attenuation = to->loop_attenuation;
    }

    if (to->alpha != from->alpha) {
        delta->delta_bits |= Q2P_ESD_ALPHA;
        delta->alpha = to->alpha;
    }
    if (to->scale != from->scale) {
        delta->delta_bits |= Q2P_ESD_SCALE;
        delta->scale = to->scale;
    }
#endif
}

static void q2repro_server_make_player_state_delta(q2proto_servercontext_t *context,
                                                   const q2proto_packed_player_state_t *from,
                                                   const q2proto_packed_player_state_t *to,
                                                   q2proto_svc_playerstate_t *delta)
{
    memset(delta, 0, sizeof(*delta));

    if (!from)
        from = &q2proto_null_packed_player_state;

    if (to->pm_type != from->pm_type) {
        delta->delta_bits |= Q2P_PSD_PM_TYPE;
        delta->pm_type = to->pm_type;
    }

    for (int c = 0; c < 3; c++) {
        q2proto_var_coords_set_float_comp(&delta->pm_origin.write.prev, c,
                                          _q2proto_valenc_bits2float(from->pm_origin[c]));
        q2proto_var_coords_set_float_comp(&delta->pm_origin.write.current, c,
                                          _q2proto_valenc_bits2float(to->pm_origin[c]));
        q2proto_var_coords_set_float_comp(&delta->pm_velocity.write.prev, c,
                                          _q2proto_valenc_bits2float(from->pm_velocity[c]));
        q2proto_var_coords_set_float_comp(&delta->pm_velocity.write.current, c,
                                          _q2proto_valenc_bits2float(to->pm_velocity[c]));
    }

    if (to->pm_time != from->pm_time) {
        delta->delta_bits |= Q2P_PSD_PM_TIME;
        delta->pm_time = to->pm_time;
    }

    if (to->pm_flags != from->pm_flags) {
        delta->delta_bits |= Q2P_PSD_PM_FLAGS;
        delta->pm_flags = to->pm_flags;
    }

    if (to->pm_gravity != from->pm_gravity) {
        delta->delta_bits |= Q2P_PSD_PM_GRAVITY;
        delta->pm_gravity = to->pm_gravity;
    }

    if (memcmp(&to->pm_delta_angles, &from->pm_delta_angles, sizeof(to->pm_delta_angles)) != 0) {
        delta->delta_bits |= Q2P_PSD_PM_DELTA_ANGLES;
        q2proto_var_angles_set_short(&delta->pm_delta_angles, to->pm_delta_angles);
    }

    if (memcmp(to->viewoffset, from->viewoffset, sizeof(to->viewoffset)) != 0) {
        delta->delta_bits |= Q2P_PSD_VIEWOFFSET;
        q2proto_var_small_offsets_set_q2repro_viewoffset_comp(&delta->viewoffset, 0, to->viewoffset[0]);
        q2proto_var_small_offsets_set_q2repro_viewoffset_comp(&delta->viewoffset, 1, to->viewoffset[1]);
        q2proto_var_small_offsets_set_q2repro_viewoffset_comp(&delta->viewoffset, 2, to->viewoffset[2]);
    }

    Q2PROTO_SET_ANGLES_DELTA(delta->viewangles, to->viewangles, from->viewangles, short);

    if (memcmp(to->kick_angles, from->kick_angles, sizeof(to->kick_angles))) {
        delta->delta_bits |= Q2P_PSD_KICKANGLES;
        q2proto_var_small_angles_set_q2repro_kick_angles_comp(&delta->kick_angles, 0, to->kick_angles[0]);
        q2proto_var_small_angles_set_q2repro_kick_angles_comp(&delta->kick_angles, 1, to->kick_angles[1]);
        q2proto_var_small_angles_set_q2repro_kick_angles_comp(&delta->kick_angles, 2, to->kick_angles[2]);
    }

    for (int c = 0; c < 4; c++) {
        if (to->blend[c] != from->blend[c]) {
            q2proto_var_color_set_byte_comp(&delta->blend.values, c, to->blend[c]);
            delta->blend.delta_bits |= BIT(c);
        }
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
        if (to->damage_blend[c] != from->damage_blend[c]) {
            q2proto_var_color_set_byte_comp(&delta->damage_blend.values, c, to->damage_blend[c]);
            delta->damage_blend.delta_bits |= BIT(c);
        }
#endif
    }

    if (to->fov != from->fov) {
        delta->delta_bits |= Q2P_PSD_FOV;
        delta->fov = to->fov;
    }

    if (to->rdflags != from->rdflags) {
        delta->delta_bits |= Q2P_PSD_RDFLAGS;
        delta->rdflags = to->rdflags;
    }

    if (to->gunframe != from->gunframe)
        delta->delta_bits |= Q2P_PSD_GUNFRAME;
    delta->gunoffset.delta_bits = 0;
    delta->gunangles.delta_bits = 0;
    for (int c = 0; c < 3; c++) {
        if (to->gunoffset[c] != from->gunoffset[c])
            delta->gunoffset.delta_bits |= BIT(c);
        if (to->gunangles[c] != from->gunangles[c])
            delta->gunangles.delta_bits |= BIT(c);
    }
    if ((delta->delta_bits & Q2P_PSD_GUNFRAME) || (delta->gunoffset.delta_bits != 0)
        || (delta->gunangles.delta_bits != 0))
    {
        delta->gunframe = to->gunframe;
        q2proto_var_small_offsets_set_q2repro_gunoffset_comp(&delta->gunoffset.values, 0, to->gunoffset[0]);
        q2proto_var_small_offsets_set_q2repro_gunoffset_comp(&delta->gunoffset.values, 1, to->gunoffset[1]);
        q2proto_var_small_offsets_set_q2repro_gunoffset_comp(&delta->gunoffset.values, 2, to->gunoffset[2]);
        q2proto_var_small_angles_set_q2repro_gunangles_comp(&delta->gunangles.values, 0, to->gunangles[0]);
        q2proto_var_small_angles_set_q2repro_gunangles_comp(&delta->gunangles.values, 1, to->gunangles[1]);
        q2proto_var_small_angles_set_q2repro_gunangles_comp(&delta->gunangles.values, 2, to->gunangles[2]);
    }

    if (to->gunindex != from->gunindex)
        delta->delta_bits |= Q2P_PSD_GUNINDEX;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    if (to->gunskin != from->gunskin)
        delta->delta_bits |= Q2P_PSD_GUNSKIN;
#endif
    if (delta->delta_bits & (Q2P_PSD_GUNINDEX | Q2P_PSD_GUNSKIN)) {
        delta->gunindex = to->gunindex;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        delta->gunskin = to->gunskin;
#endif
    }

    for (int i = 0; i < Q2PROTO_STATS; i++) {
        if (to->stats[i] != from->stats[i]) {
            delta->statbits |= BIT_ULL(i);
            delta->stats[i] = to->stats[i];
        }
    }

#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
    if (to->gunrate != from->gunrate) {
        delta->delta_bits |= Q2P_PSD_GUNRATE;
        delta->gunrate = to->gunrate;
    }

    if (to->pm_viewheight != from->pm_viewheight) {
        delta->delta_bits |= Q2P_PSD_PM_VIEWHEIGHT;
        delta->pm_viewheight = to->pm_viewheight;
    }
#endif
}

static q2proto_error_t q2repro_server_write_serverdata(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                       const q2proto_svc_serverdata_t *serverdata);
static q2proto_error_t q2repro_server_write_spawnbaseline(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                          const q2proto_svc_spawnbaseline_t *spawnbaseline);
static q2proto_error_t q2repro_server_write_download(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                     const q2proto_svc_download_t *download);
static q2proto_error_t q2repro_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                  const q2proto_svc_frame_t *frame);
static q2proto_error_t q2repro_server_write_frame_entity_delta(
    q2proto_servercontext_t *context, uintptr_t io_arg, const q2proto_svc_frame_entity_delta_t *frame_entity_delta);

static q2proto_error_t q2repro_server_write(q2proto_servercontext_t *context, uintptr_t io_arg,
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
        return q2proto_common_server_write_sound(Q2P_PROTOCOL_MULTICAST_FLOAT, io_arg, &svc_message->sound);

    case Q2P_SVC_PRINT:
        return q2proto_common_server_write_print(io_arg, &svc_message->print);

    case Q2P_SVC_STUFFTEXT:
        return q2proto_common_server_write_stufftext(io_arg, &svc_message->stufftext);

    case Q2P_SVC_SERVERDATA:
        return q2repro_server_write_serverdata(context, io_arg, &svc_message->serverdata);

    case Q2P_SVC_CONFIGSTRING:
        return q2proto_common_server_write_configstring(io_arg, &svc_message->configstring);

    case Q2P_SVC_SPAWNBASELINE:
        return q2repro_server_write_spawnbaseline(context, io_arg, &svc_message->spawnbaseline);

    case Q2P_SVC_CENTERPRINT:
        return q2proto_common_server_write_centerprint(io_arg, &svc_message->centerprint);

    case Q2P_SVC_DOWNLOAD:
        return q2repro_server_write_download(context, io_arg, &svc_message->download);

    case Q2P_SVC_FRAME:
        return q2repro_server_write_frame(context, io_arg, &svc_message->frame);

    case Q2P_SVC_FRAME_ENTITY_DELTA:
        return q2repro_server_write_frame_entity_delta(context, io_arg, &svc_message->frame_entity_delta);

    case Q2P_SVC_LAYOUT:
        // Although typically written by the game, this is useful when writing demos
        return q2proto_common_server_write_layout(io_arg, &svc_message->layout);

    case Q2P_SVC_FOG:
        // Although typically written by the game, this is useful when writing demos
        return q2proto_q2repro_server_write_fog(io_arg, &svc_message->fog);

    default:
        break;
    }

    /* The following messages are currently not covered,
     * as they're actually sent by game code:
     *  muzzleflash
     *  muzzleflash2
     *  temp_entity
     *  inventory
     */

    return Q2P_ERR_NOT_IMPLEMENTED;
}

static q2proto_error_t q2repro_server_write_serverdata(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                       const q2proto_svc_serverdata_t *serverdata)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_serverdata);
    WRITE_CHECKED(server_write, io_arg, i32, PROTOCOL_Q2REPRO);
    WRITE_CHECKED(server_write, io_arg, i32, serverdata->servercount);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->attractloop);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->gamedir);
    WRITE_CHECKED(server_write, io_arg, i16, serverdata->clientnum);
    WRITE_CHECKED(server_write, io_arg, string, &serverdata->levelname);
    WRITE_CHECKED(server_write, io_arg, u16, serverdata->protocol_version);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->q2pro.server_state);
    uint16_t q2pro_flags = 0;
    if (serverdata->strafejump_hack)
        q2pro_flags |= Q2PRO_PF_STRAFEJUMP_HACK;
    if (serverdata->q2pro.qw_mode)
        q2pro_flags |= Q2PRO_PF_QW_MODE;
    if (serverdata->q2pro.waterjump_hack)
        q2pro_flags |= Q2PRO_PF_WATERJUMP_HACK;
    if (serverdata->q2pro.extensions)
        q2pro_flags |= Q2PRO_PF_EXTENSIONS;
    if (serverdata->q2pro.extensions_v2)
        q2pro_flags |= Q2PRO_PF_EXTENSIONS_2;
    if (serverdata->q2repro.game3_compat)
        q2pro_flags |= Q2REPRO_PF_GAME3_COMPAT;
    WRITE_CHECKED(client_read, io_arg, u16, q2pro_flags);
    WRITE_CHECKED(server_write, io_arg, u8, serverdata->q2repro.server_fps);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2proto_q2repro_server_write_entity_state_delta(
    q2proto_servercontext_t *context, uintptr_t io_arg, uint16_t entnum,
    const q2proto_entity_state_delta_t *entity_state_delta)
{
    uint64_t bits = 0;

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
    if (bits & (U_ANGLE1 | U_ANGLE2 | U_ANGLE3))
        bits |= U_ANGLE16;


    if (entity_state_delta->delta_bits & Q2P_ESD_SKINNUM)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->skinnum, U_SKIN8, U_SKIN16, true);

    if (entity_state_delta->delta_bits & Q2P_ESD_FRAME) {
        if (entity_state_delta->frame >= 256)
            bits |= U_FRAME16;
        else
            bits |= U_FRAME8;
    }

    if (entity_state_delta->delta_bits & Q2P_ESD_EFFECTS)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->effects, U_EFFECTS8, U_EFFECTS16, true);

#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    if (entity_state_delta->delta_bits & Q2P_ESD_EFFECTS_MORE)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->effects_more, U_MOREFX8, U_MOREFX16, true);
#endif

    if (entity_state_delta->delta_bits & Q2P_ESD_RENDERFX)
        bits |= q2proto_common_choose_width_flags(entity_state_delta->renderfx, U_RENDERFX8, U_RENDERFX16, true);

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
    if (((bits & U_MODEL) && (entity_state_delta->modelindex > 255))
        || ((bits & U_MODEL2) && (entity_state_delta->modelindex2 > 255))
        || ((bits & U_MODEL3) && (entity_state_delta->modelindex3 > 255))
        || ((bits & U_MODEL4) && (entity_state_delta->modelindex4 > 255)))
        bits |= U_MODEL16;

    if (entity_state_delta->delta_bits & Q2P_ESD_SOUND)
        bits |= U_SOUND;

    if (entity_state_delta->delta_bits & Q2P_ESD_OLD_ORIGIN)
        bits |= U_OLDORIGIN;

    if (entity_state_delta->delta_bits & Q2P_ESD_ALPHA)
        bits |= U_ALPHA;

    if (entity_state_delta->delta_bits & Q2P_ESD_SCALE)
        bits |= U_SCALE;

    //----------

    q2proto_common_server_write_entity_bits(io_arg, bits, entnum);

    if (bits & U_MODEL16) {
        if (bits & U_MODEL)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex);
        if (bits & U_MODEL2)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex2);
        if (bits & U_MODEL3)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex3);
        if (bits & U_MODEL4)
            WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->modelindex4);
    } else {
        if (bits & U_MODEL)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex);
        if (bits & U_MODEL2)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex2);
        if (bits & U_MODEL3)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->modelindex3);
        if (bits & U_MODEL4)
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
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&entity_state_delta->origin.write.current, 0));
    if (bits & U_ORIGIN2)
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&entity_state_delta->origin.write.current, 1));
    if (bits & U_ORIGIN3)
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&entity_state_delta->origin.write.current, 2));

    if (bits & U_ANGLE16) {
        if (bits & U_ANGLE1)
            WRITE_CHECKED(server_write, io_arg, i16,
                          q2proto_var_angles_get_short_comp(&entity_state_delta->angle.values, 0));
        if (bits & U_ANGLE2)
            WRITE_CHECKED(server_write, io_arg, i16,
                          q2proto_var_angles_get_short_comp(&entity_state_delta->angle.values, 1));
        if (bits & U_ANGLE3)
            WRITE_CHECKED(server_write, io_arg, i16,
                          q2proto_var_angles_get_short_comp(&entity_state_delta->angle.values, 2));
    } else {
        if (bits & U_ANGLE1)
            WRITE_CHECKED(server_write, io_arg, u8,
                          q2proto_var_angles_get_char_comp(&entity_state_delta->angle.values, 0));
        if (bits & U_ANGLE2)
            WRITE_CHECKED(server_write, io_arg, u8,
                          q2proto_var_angles_get_char_comp(&entity_state_delta->angle.values, 1));
        if (bits & U_ANGLE3)
            WRITE_CHECKED(server_write, io_arg, u8,
                          q2proto_var_angles_get_char_comp(&entity_state_delta->angle.values, 2));
    }

    if (bits & U_OLDORIGIN) {
        CHECKED_IO(server_write, io_arg, q2protoio_write_var_coords_float(io_arg, &entity_state_delta->old_origin),
                   "write old_origin");
    }

    if (bits & U_SOUND) {
        uint16_t sound_word = entity_state_delta->sound;
        if (entity_state_delta->delta_bits & Q2P_ESD_LOOP_ATTENUATION)
            sound_word |= SOUND_FLAG_ATTENUATION;
        if (entity_state_delta->delta_bits & Q2P_ESD_LOOP_VOLUME)
            sound_word |= SOUND_FLAG_VOLUME;
        WRITE_CHECKED(server_write, io_arg, u16, sound_word);
#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        if (sound_word & SOUND_FLAG_VOLUME)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->loop_volume);
        if (sound_word & SOUND_FLAG_ATTENUATION)
            WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->loop_attenuation);
#endif
    }
    if (bits & U_EVENT)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->event);
    if (bits & U_SOLID)
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->solid);

#if Q2PROTO_ENTITY_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
    if ((bits & U_MOREFX32) == U_MOREFX32)
        WRITE_CHECKED(server_write, io_arg, u32, entity_state_delta->effects_more);
    else if (bits & U_MOREFX16)
        WRITE_CHECKED(server_write, io_arg, u16, entity_state_delta->effects_more);
    else if (bits & U_MOREFX8)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->effects_more);

    if (bits & U_ALPHA)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->alpha);

    if (bits & U_SCALE)
        WRITE_CHECKED(server_write, io_arg, u8, entity_state_delta->scale);
#endif

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_write_spawnbaseline(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                          const q2proto_svc_spawnbaseline_t *spawnbaseline)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_spawnbaseline);
    CHECKED(server_write, io_arg,
            q2proto_q2repro_server_write_entity_state_delta(context, io_arg, spawnbaseline->entnum,
                                                            &spawnbaseline->delta_state));
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_write_download(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                     const q2proto_svc_download_t *download)
{
    WRITE_CHECKED(server_write, io_arg, u8, download->compressed ? svc_r1q2_zdownload : svc_download);
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

static q2proto_error_t q2repro_server_write_playerstate(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                        const q2proto_svc_playerstate_t *playerstate,
                                                        uint8_t *extraflags)
{
    uint16_t flags = 0;
    *extraflags = 0;

    unsigned origin_differs = q2proto_maybe_diff_coords_write_differs_float(&playerstate->pm_origin);
    unsigned velocity_differs = q2proto_maybe_diff_coords_write_differs_float(&playerstate->pm_velocity);
    if (playerstate->delta_bits & Q2P_PSD_PM_TYPE)
        flags |= PS_M_TYPE;
    if (origin_differs & (BIT(0) | BIT(1)))
        flags |= PS_M_ORIGIN;
    if (origin_differs & BIT(2))
        *extraflags |= EPS_M_ORIGIN2;
    if (velocity_differs & (BIT(0) | BIT(1)))
        flags |= PS_M_VELOCITY;
    if (velocity_differs & BIT(2))
        *extraflags |= EPS_M_VELOCITY2;
    if (playerstate->delta_bits & Q2P_PSD_PM_TIME)
        flags |= PS_M_TIME;
    if (playerstate->delta_bits & Q2P_PSD_PM_FLAGS)
        flags |= PS_M_FLAGS;
    if (playerstate->delta_bits & Q2P_PSD_PM_GRAVITY)
        flags |= PS_M_GRAVITY;
    if (playerstate->delta_bits & Q2P_PSD_PM_DELTA_ANGLES)
        flags |= PS_M_DELTA_ANGLES;
    if (playerstate->delta_bits & Q2P_PSD_PM_VIEWHEIGHT)
        flags |= PS_RR_VIEWHEIGHT;
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
        flags |= PS_BLEND;
#endif
    if (playerstate->delta_bits & Q2P_PSD_FOV)
        flags |= PS_FOV;
    if (playerstate->delta_bits & Q2P_PSD_RDFLAGS)
        flags |= PS_RDFLAGS;
    if (playerstate->delta_bits & Q2P_PSD_GUNINDEX)
        flags |= PS_WEAPONINDEX;
    if (playerstate->delta_bits & Q2P_PSD_GUNFRAME)
        flags |= PS_WEAPONFRAME;
    if (playerstate->gunoffset.delta_bits != 0)
        *extraflags |= EPS_GUNOFFSET;
    if (playerstate->gunangles.delta_bits != 0)
        *extraflags |= EPS_GUNANGLES;
    if (playerstate->statbits != 0)
        *extraflags |= EPS_STATS;
    if (playerstate->delta_bits & Q2P_PSD_GUNRATE)
        *extraflags |= EPS_GUNRATE;
    if (playerstate->delta_bits & Q2P_PSD_CLIENTNUM)
        *extraflags |= EPS_CLIENTNUM;
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
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&playerstate->pm_origin.write.current, 0));
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&playerstate->pm_origin.write.current, 1));
    }
    if (*extraflags & EPS_M_ORIGIN2)
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&playerstate->pm_origin.write.current, 2));

    if (flags & PS_M_VELOCITY) {
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&playerstate->pm_velocity.write.current, 0));
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&playerstate->pm_velocity.write.current, 1));
    }
    if (*extraflags & EPS_M_VELOCITY2)
        WRITE_CHECKED(server_write, io_arg, float,
                      q2proto_var_coords_get_float_comp(&playerstate->pm_velocity.write.current, 2));

    if (flags & PS_M_TIME)
        WRITE_CHECKED(server_write, io_arg, u16, playerstate->pm_time);

    if (flags & PS_M_FLAGS)
        WRITE_CHECKED(server_write, io_arg, u16, playerstate->pm_flags);

    if (flags & PS_M_GRAVITY)
        WRITE_CHECKED(server_write, io_arg, i16, playerstate->pm_gravity);

    if (flags & PS_M_DELTA_ANGLES) {
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->pm_delta_angles, 0));
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->pm_delta_angles, 1));
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->pm_delta_angles, 2));
    }

    if (flags & PS_VIEWOFFSET) {
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_viewoffset_comp(&playerstate->viewoffset, 0));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_viewoffset_comp(&playerstate->viewoffset, 1));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_viewoffset_comp(&playerstate->viewoffset, 2));
    }

    if (flags & PS_VIEWANGLES) {
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 0));
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 1));
    }
    if (*extraflags & EPS_VIEWANGLE2)
        WRITE_CHECKED(server_write, io_arg, i16, q2proto_var_angles_get_short_comp(&playerstate->viewangles.values, 2));

    if (flags & PS_KICKANGLES) {
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_kick_angles_comp(&playerstate->kick_angles, 0));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_kick_angles_comp(&playerstate->kick_angles, 1));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_kick_angles_comp(&playerstate->kick_angles, 2));
    }

    if (flags & PS_WEAPONINDEX) {
        uint16_t gun_index_and_skin = playerstate->gunindex;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED
        gun_index_and_skin |= (playerstate->gunskin << Q2PRO_GUNINDEX_BITS);
#endif
        WRITE_CHECKED(server_write, io_arg, u16, gun_index_and_skin);
    }

    if (flags & PS_WEAPONFRAME)
        WRITE_CHECKED(server_write, io_arg, u16, playerstate->gunframe);
    if (*extraflags & EPS_GUNOFFSET) {
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_gunoffset_comp(&playerstate->gunoffset.values, 0));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_gunoffset_comp(&playerstate->gunoffset.values, 1));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_offsets_get_q2repro_gunoffset_comp(&playerstate->gunoffset.values, 2));
    }
    if (*extraflags & EPS_GUNANGLES) {
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_gunangles_comp(&playerstate->gunangles.values, 0));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_gunangles_comp(&playerstate->gunangles.values, 1));
        WRITE_CHECKED(server_write, io_arg, i16,
                      q2proto_var_small_angles_get_q2repro_gunangles_comp(&playerstate->gunangles.values, 2));
    }

    if (flags & PS_BLEND) {
        const q2proto_color_delta_t *damage_blend;
#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_Q2PRO_EXTENDED_V2
        damage_blend = &playerstate->damage_blend;
#else
        const q2proto_color_delta_t null_blend = {0};
        damage_blend = &null_blend;
#endif
        CHECKED(server_write, io_arg, server_write_q2pro_extv2_blends(io_arg, &playerstate->blend, damage_blend));
    }
    if (flags & PS_FOV)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->fov);
    if (flags & PS_RDFLAGS)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->rdflags);

    // send stats
    if (*extraflags & EPS_STATS) {
        int numstats = 64;
        WRITE_CHECKED(server_write, io_arg, u64, playerstate->statbits);
        for (int i = 0; i < numstats; i++)
            if (playerstate->statbits & BIT_ULL(i))
                WRITE_CHECKED(server_write, io_arg, i16, playerstate->stats[i]);
    }

#if Q2PROTO_PLAYER_STATE_FEATURES >= Q2PROTO_FEATURES_RERELEASE
    if (*extraflags & EPS_GUNRATE)
        WRITE_CHECKED(server_write, io_arg, u8, playerstate->gunrate);

    if (flags & PS_RR_VIEWHEIGHT)
        WRITE_CHECKED(server_write, io_arg, i8, playerstate->pm_viewheight);
#endif

    if (*extraflags & EPS_CLIENTNUM)
        WRITE_CHECKED(server_write, io_arg, i16, playerstate->clientnum);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_write_frame(q2proto_servercontext_t *context, uintptr_t io_arg,
                                                  const q2proto_svc_frame_t *frame)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_frame);

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

    WRITE_CHECKED(server_write, io_arg, u8, frame->q2pro_frame_flags);
    // save the position for the extraflags byte
    void *extraflags;
    CHECKED_IO(client_write, io_arg, extraflags = q2protoio_write_reserve_raw(io_arg, 1), "reserve extraflags byte");

    // write areabits
    WRITE_CHECKED(server_write, io_arg, u8, frame->areabits_len);
    void *areabits;
    CHECKED_IO(server_write, io_arg, areabits = q2protoio_write_reserve_raw(io_arg, frame->areabits_len),
               "reserve areabits");
    memcpy(areabits, frame->areabits, frame->areabits_len);

    CHECKED(server_write, io_arg,
            q2repro_server_write_playerstate(context, io_arg, &frame->playerstate, (uint8_t *)extraflags));

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_write_frame_entity_delta(
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

    return q2proto_q2repro_server_write_entity_state_delta(context, io_arg, frame_entity_delta->newnum,
                                                           &frame_entity_delta->entity_delta);
}

q2proto_error_t q2proto_q2repro_server_write_fog(uintptr_t io_arg, const q2proto_svc_fog_t *fog)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_rr_fog);

    unsigned int bits = 0;
    if (fog->flags & Q2P_FOG_DENSITY_SKYFACTOR)
        bits |= FOG_RR_BIT_DENSITY;
    if (fog->global.color.delta_bits & BIT(0))
        bits |= FOG_RR_BIT_R;
    if (fog->global.color.delta_bits & BIT(1))
        bits |= FOG_RR_BIT_G;
    if (fog->global.color.delta_bits & BIT(2))
        bits |= FOG_RR_BIT_B;
    if (fog->flags & Q2P_FOG_TIME)
        bits |= FOG_RR_BIT_TIME;

    if (fog->flags & Q2P_HEIGHTFOG_FALLOFF)
        bits |= FOG_RR_BIT_HEIGHTFOG_FALLOFF;
    if (fog->flags & Q2P_HEIGHTFOG_DENSITY)
        bits |= FOG_RR_BIT_HEIGHTFOG_DENSITY;

    if (fog->height.start_color.delta_bits & BIT(0))
        bits |= FOG_RR_BIT_HEIGHTFOG_START_R;
    if (fog->height.start_color.delta_bits & BIT(1))
        bits |= FOG_RR_BIT_HEIGHTFOG_START_G;
    if (fog->height.start_color.delta_bits & BIT(2))
        bits |= FOG_RR_BIT_HEIGHTFOG_START_B;
    if (fog->flags & Q2P_HEIGHTFOG_START_DIST)
        bits |= FOG_RR_BIT_HEIGHTFOG_START_DIST;

    if (fog->height.end_color.delta_bits & BIT(0))
        bits |= FOG_RR_BIT_HEIGHTFOG_END_R;
    if (fog->height.end_color.delta_bits & BIT(1))
        bits |= FOG_RR_BIT_HEIGHTFOG_END_G;
    if (fog->height.end_color.delta_bits & BIT(2))
        bits |= FOG_RR_BIT_HEIGHTFOG_END_B;
    if (fog->flags & Q2P_HEIGHTFOG_END_DIST)
        bits |= FOG_RR_BIT_HEIGHTFOG_END_DIST;

    if ((bits >> 8) != 0)
        bits |= FOG_RR_BIT_MORE_BITS;

    WRITE_CHECKED(server_write, io_arg, u8, bits & 0xff);
    if (bits & FOG_RR_BIT_MORE_BITS)
        WRITE_CHECKED(server_write, io_arg, u8, bits >> 8);

    if (bits & FOG_RR_BIT_DENSITY) {
        WRITE_CHECKED(server_write, io_arg, float, q2proto_var_fraction_get_float(&fog->global.density));
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_fraction_get_byte(&fog->global.skyfactor));
    }
    if (bits & FOG_RR_BIT_R)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->global.color.values, 0));
    if (bits & FOG_RR_BIT_G)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->global.color.values, 1));
    if (bits & FOG_RR_BIT_B)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->global.color.values, 2));
    if (bits & FOG_RR_BIT_TIME)
        WRITE_CHECKED(server_write, io_arg, u16, fog->global.time);

    if (bits & FOG_RR_BIT_HEIGHTFOG_FALLOFF)
        WRITE_CHECKED(server_write, io_arg, float, q2proto_var_fraction_get_float(&fog->height.falloff));
    if (bits & FOG_RR_BIT_HEIGHTFOG_DENSITY)
        WRITE_CHECKED(server_write, io_arg, float, q2proto_var_fraction_get_float(&fog->height.density));

    if (bits & FOG_RR_BIT_HEIGHTFOG_START_R)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->height.start_color.values, 0));
    if (bits & FOG_RR_BIT_HEIGHTFOG_START_G)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->height.start_color.values, 1));
    if (bits & FOG_RR_BIT_HEIGHTFOG_START_B)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->height.start_color.values, 2));
    if (bits & FOG_RR_BIT_HEIGHTFOG_START_DIST)
        WRITE_CHECKED(server_write, io_arg, i32, q2proto_var_coord_get_int_unscaled(&fog->height.start_dist));

    if (bits & FOG_RR_BIT_HEIGHTFOG_END_R)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->height.end_color.values, 0));
    if (bits & FOG_RR_BIT_HEIGHTFOG_END_G)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->height.end_color.values, 1));
    if (bits & FOG_RR_BIT_HEIGHTFOG_END_B)
        WRITE_CHECKED(server_write, io_arg, u8, q2proto_var_color_get_byte_comp(&fog->height.end_color.values, 2));
    if (bits & FOG_RR_BIT_HEIGHTFOG_END_DIST)
        WRITE_CHECKED(server_write, io_arg, i32, q2proto_var_coord_get_int_unscaled(&fog->height.end_dist));

    return Q2P_ERR_SUCCESS;
}


// Max size of a Q2rePRO entity baseline
#define WRITE_GAMESTATE_BASELINE_SIZE \
    (1    /* command byte */          \
     + 7  /* bits & number */         \
     + 8  /* model indices */         \
     + 2  /* frame */                 \
     + 4  /* skin */                  \
     + 8  /* effects + morefx */      \
     + 4  /* renderfx */              \
     + 12 /* origin */                \
     + 6  /* angles */                \
     + 12 /* old_origin */            \
     + 2  /* sound */                 \
     + 1  /* loop volume */           \
     + 1  /* loop attenuation */      \
     + 1  /* event */                 \
     + 4  /* solid */                 \
     + 1  /* alpha */                 \
     + 1  /* scale */                 \
    )

static q2proto_error_t q2repro_server_write_gamestate(q2proto_servercontext_t *context,
                                                      q2protoio_deflate_args_t *deflate_args, uintptr_t io_arg,
                                                      const q2proto_gamestate_t *gamestate)
{
    unsigned int max_configstrings = max_configstrings_for_game(context->server_info->game_api);

    q2proto_maybe_zpacket_t zpacket_state;
    q2proto_maybe_zpacket_begin(context, deflate_args, io_arg, &zpacket_state, &io_arg);
    // can ignore errors, as it'll fall back to uncompressed writing

    if (context->gamestate_pos < gamestate->num_configstrings) {
        WRITE_CHECKED(server_write, io_arg, u8, svc_q2repro_configstringstream);

        // Write configstrings
        while (context->gamestate_pos < gamestate->num_configstrings) {
            const q2proto_svc_configstring_t *cfgstr = gamestate->configstrings + context->gamestate_pos;
            size_t configstring_size = 2 /* index */ + cfgstr->value.len + 1 /* string */ + 2 /* end marker */;
            if (q2protoio_write_available(io_arg) < configstring_size) {
                WRITE_CHECKED(server_write, io_arg, u16, max_configstrings);
                goto not_enough_packet_space;
            }

            WRITE_CHECKED(server_write, io_arg, u16, cfgstr->index);
            WRITE_CHECKED(server_write, io_arg, string, &cfgstr->value);
            context->gamestate_pos++;
        }
        WRITE_CHECKED(server_write, io_arg, u16, max_configstrings);
    }

    if (context->gamestate_pos - gamestate->num_configstrings < gamestate->num_spawnbaselines) {
        WRITE_CHECKED(server_write, io_arg, u8, svc_q2repro_baselinestream);

        // Write spawn baselines
        size_t baseline_num;
        while ((baseline_num = context->gamestate_pos - gamestate->num_configstrings) < gamestate->num_spawnbaselines) {
            const q2proto_svc_spawnbaseline_t *baseline = gamestate->spawnbaselines + baseline_num;
            if (q2protoio_write_available(io_arg) < WRITE_GAMESTATE_BASELINE_SIZE + 2 /* end marker */) {
                WRITE_CHECKED(server_write, io_arg, u16, 0); // zero bits + entnum
                goto not_enough_packet_space;
            }

            CHECKED(server_write, io_arg,
                    q2proto_q2repro_server_write_entity_state_delta(context, io_arg, baseline->entnum,
                                                                    &baseline->delta_state));
            context->gamestate_pos++;
        }

        WRITE_CHECKED(server_write, io_arg, u16, 0); // zero bits + entnum
    }

    // Game state written successfully, reset state
    context->gamestate_pos = 0;

    return q2proto_maybe_zpacket_end(&zpacket_state, io_arg);

not_enough_packet_space:
    q2proto_error_t zpacket_err = q2proto_maybe_zpacket_end(&zpacket_state, io_arg);
    if (zpacket_err != Q2P_ERR_SUCCESS)
        return zpacket_err;
    return Q2P_ERR_NOT_ENOUGH_PACKET_SPACE;
}

#undef WRITE_GAMESTATE_BASELINE_SIZE

static q2proto_error_t q2repro_server_read_move(uintptr_t io_arg, q2proto_clc_move_t *move);
static q2proto_error_t q2repro_server_read_batch_move(uintptr_t io_arg, bool nodelta, q2proto_clc_batch_move_t *move);
static q2proto_error_t q2repro_server_read_setting(uintptr_t io_arg, q2proto_clc_setting_t *setting);
static q2proto_error_t q2repro_server_read_userinfo_delta(uintptr_t io_arg,
                                                          q2proto_clc_userinfo_delta_t *userinfo_delta);

static q2proto_error_t q2repro_server_read(q2proto_servercontext_t *context, uintptr_t io_arg,
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
        return q2repro_server_read_move(io_arg, &clc_message->move);

    case clc_userinfo:
        clc_message->type = Q2P_CLC_USERINFO;
        return q2proto_common_server_read_userinfo(io_arg, &clc_message->userinfo);

    case clc_stringcmd:
        clc_message->type = Q2P_CLC_STRINGCMD;
        return q2proto_common_server_read_stringcmd(io_arg, &clc_message->stringcmd);

    case clc_r1q2_setting:
        clc_message->type = Q2P_CLC_SETTING;
        return q2repro_server_read_setting(io_arg, &clc_message->setting);

    case clc_q2pro_move_nodelta:
    case clc_q2pro_move_batched:
        clc_message->type = Q2P_CLC_BATCH_MOVE;
        return q2repro_server_read_batch_move(io_arg, command == clc_q2pro_move_nodelta, &clc_message->batch_move);

    case clc_q2pro_userinfo_delta:
        clc_message->type = Q2P_CLC_USERINFO_DELTA;
        return q2repro_server_read_userinfo_delta(io_arg, &clc_message->userinfo_delta);
    }

    return HANDLE_ERROR(client_read, io_arg, Q2P_ERR_BAD_COMMAND, "%s: bad server command %d", __func__, command);
}

static q2proto_error_t q2repro_server_read_move_delta(uintptr_t io_arg, q2proto_clc_move_delta_t *move_delta)
{
    uint8_t bits;
    READ_CHECKED(server_read, io_arg, bits, u8);

    if (delta_bits_check(bits, CM_ANGLE1, &move_delta->delta_bits, Q2P_CMD_ANGLE0))
        READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 0);
    if (delta_bits_check(bits, CM_ANGLE2, &move_delta->delta_bits, Q2P_CMD_ANGLE1))
        READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 1);
    if (delta_bits_check(bits, CM_ANGLE3, &move_delta->delta_bits, Q2P_CMD_ANGLE2))
        READ_CHECKED_VAR_ANGLES_COMP_16(server_read, io_arg, &move_delta->angles, 2);

    if (delta_bits_check(bits, CM_FORWARD, &move_delta->delta_bits, Q2P_CMD_MOVE_FORWARD)) {
        int16_t coord;
        READ_CHECKED(server_read, io_arg, coord, i16);
        q2proto_var_coords_set_float_comp(&move_delta->move, 0, coord);
    }
    if (delta_bits_check(bits, CM_SIDE, &move_delta->delta_bits, Q2P_CMD_MOVE_SIDE)) {
        int16_t coord;
        READ_CHECKED(server_read, io_arg, coord, i16);
        q2proto_var_coords_set_float_comp(&move_delta->move, 1, coord);
    }
    if (delta_bits_check(bits, CM_UP, &move_delta->delta_bits, Q2P_CMD_MOVE_UP))
        return Q2P_ERR_BAD_DATA;

    if (delta_bits_check(bits, CM_BUTTONS, &move_delta->delta_bits, Q2P_CMD_BUTTONS))
        READ_CHECKED(server_read, io_arg, move_delta->buttons, u8);
    if (delta_bits_check(bits, CM_IMPULSE, &move_delta->delta_bits, Q2P_CMD_IMPULSE))
        READ_CHECKED(server_read, io_arg, move_delta->impulse, u8);

    READ_CHECKED(server_read, io_arg, move_delta->msec, u8);
    READ_CHECKED(server_read, io_arg, move_delta->lightlevel, u8);

    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_read_move(uintptr_t io_arg, q2proto_clc_move_t *move)
{
    READ_CHECKED(server_read, io_arg, move->lastframe, i32);
    for (int i = 0; i < 3; i++) {
        CHECKED(server_read, io_arg, q2repro_server_read_move_delta(io_arg, &move->moves[i]));
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_read_batch_move_delta_angle(bitreader_t *bitreader,
                                                                  const q2proto_clc_move_delta_t *prev_move_delta,
                                                                  q2proto_clc_move_delta_t *move_delta, int angle_idx)
{
    int delta_flag = 0;
    CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, 1, &delta_flag));
    if (delta_flag) {
        int angles_delta = 0;
        CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, -8, &angles_delta));
        int prev_angle = prev_move_delta ? q2proto_var_angles_get_short_comp(&prev_move_delta->angles, angle_idx) : 0;
        q2proto_var_angles_set_short_comp(&move_delta->angles, angle_idx, prev_angle + angles_delta);
    } else {
        int angle_value = 0;
        CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, -16, &angle_value));
        q2proto_var_angles_set_short_comp(&move_delta->angles, angle_idx, angle_value);
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_read_batch_move_delta(bitreader_t *bitreader,
                                                            const q2proto_clc_move_delta_t *prev_move_delta,
                                                            q2proto_clc_move_delta_t *move_delta)
{
    move_delta->delta_bits = 0;
    // angles may be used by any future move delta, so make sure they're always copied over
    if (prev_move_delta)
        move_delta->angles = prev_move_delta->angles;
    else
        memset(&move_delta->angles, 0, sizeof(move_delta->angles));
    move_delta->msec = prev_move_delta ? prev_move_delta->msec : 0; // there's no delta bit for msec

    int has_contents = 0;
    CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, 1, &has_contents));
    if (!has_contents)
        return Q2P_ERR_SUCCESS;

    int bits = 0;
    CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, 8, &bits));

    if (delta_bits_check(bits, CM_ANGLE1, &move_delta->delta_bits, Q2P_CMD_ANGLE0))
        CHECKED(server_read, bitreader->io_arg,
                q2repro_server_read_batch_move_delta_angle(bitreader, prev_move_delta, move_delta, 0));
    if (delta_bits_check(bits, CM_ANGLE2, &move_delta->delta_bits, Q2P_CMD_ANGLE1))
        CHECKED(server_read, bitreader->io_arg,
                q2repro_server_read_batch_move_delta_angle(bitreader, prev_move_delta, move_delta, 1));
    if (delta_bits_check(bits, CM_ANGLE3, &move_delta->delta_bits, Q2P_CMD_ANGLE2)) {
        int angle_value = 0;
        CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, -16, &angle_value));
        q2proto_var_angles_set_short_comp(&move_delta->angles, 2, angle_value);
    }

    if (delta_bits_check(bits, CM_FORWARD, &move_delta->delta_bits, Q2P_CMD_MOVE_FORWARD)) {
        int move_value = 0;
        CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, -10, &move_value));
        q2proto_var_coords_set_int_unscaled_comp(&move_delta->move, 0, move_value);
    }
    if (delta_bits_check(bits, CM_SIDE, &move_delta->delta_bits, Q2P_CMD_MOVE_SIDE)) {
        int move_value = 0;
        CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, -10, &move_value));
        q2proto_var_coords_set_int_unscaled_comp(&move_delta->move, 1, move_value);
    }
    if (delta_bits_check(bits, CM_UP, &move_delta->delta_bits, Q2P_CMD_MOVE_UP))
        return Q2P_ERR_BAD_DATA;

    if (delta_bits_check(bits, CM_BUTTONS, &move_delta->delta_bits, Q2P_CMD_BUTTONS)) {
        int buttons_value = 0;
        CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, 8, &buttons_value));
        move_delta->buttons = buttons_value;
    }
    if (bits & CM_IMPULSE) {
        int msec_value = 0;
        CHECKED(server_read, bitreader->io_arg, bitreader_read(bitreader, 8, &msec_value));
        move_delta->msec = msec_value;
    } else
        move_delta->msec = prev_move_delta ? prev_move_delta->msec : 0;
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_read_batch_move(uintptr_t io_arg, bool nodelta, q2proto_clc_batch_move_t *move)
{
    if (nodelta)
        move->lastframe = -1;
    else
        READ_CHECKED(server_read, io_arg, move->lastframe, i32);

    READ_CHECKED(server_read, io_arg, move->num_dups, u8);
    if (move->num_dups >= Q2PROTO_MAX_CLC_BATCH_MOVE_FRAMES - 1)
        return Q2P_ERR_BAD_DATA;

    uint8_t lightlevel;
    READ_CHECKED(server_read, io_arg, lightlevel, u8);

    bitreader_t bitreader;
    bitreader_init(&bitreader, io_arg);
    const q2proto_clc_move_delta_t *prev_move_delta = NULL;
    for (int i = 0; i <= move->num_dups; i++) {
        q2proto_clc_batch_move_frame_t *move_frame = &move->batch_frames[i];
        int num_cmds_bits = 0;
        CHECKED(server_read, io_arg, bitreader_read(&bitreader, 5, &num_cmds_bits));
        move_frame->num_cmds = num_cmds_bits;
        for (int j = 0; j < move_frame->num_cmds; j++) {
            CHECKED(server_read, io_arg,
                    q2repro_server_read_batch_move_delta(&bitreader, prev_move_delta, &move_frame->moves[j]));
            move_frame->moves[j].lightlevel = lightlevel;
            prev_move_delta = &move_frame->moves[j];
        }
    }
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_read_setting(uintptr_t io_arg, q2proto_clc_setting_t *setting)
{
    READ_CHECKED(server_read, io_arg, setting->index, i16);
    READ_CHECKED(server_read, io_arg, setting->value, i16);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_server_read_userinfo_delta(uintptr_t io_arg,
                                                          q2proto_clc_userinfo_delta_t *userinfo_delta)
{
    READ_CHECKED(server_read, io_arg, userinfo_delta->name, string);
    READ_CHECKED(server_read, io_arg, userinfo_delta->value, string);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t q2repro_download_begin(q2proto_servercontext_t *context, q2proto_server_download_state_t *state,
                                              q2proto_download_compress_t compress,
                                              q2protoio_deflate_args_t *deflate_args)
{
    if (state->total_size > 0) {
        if (compress == Q2PROTO_DOWNLOAD_COMPRESS_RAW) {
            if (!context->features.download_compress_raw)
                return Q2P_ERR_RAW_COMPRESS_NOT_SUPPORTED;
            state->compress = Q2PROTO_DOWNLOAD_DATA_RAW_DEFLATE;
            return Q2P_ERR_SUCCESS;
        } else if (compress == Q2PROTO_DOWNLOAD_COMPRESS_AUTO && Q2PROTO_COMPRESSION_DEFLATE
                   && context->features.enable_deflate && state->total_size > 0)
        {
            state->compress = Q2PROTO_DOWNLOAD_DATA_COMPRESS;
            state->deflate_args = deflate_args;
        }
    }
    return Q2P_ERR_SUCCESS;
}

#define SVC_ZDOWNLOAD_SIZE 4

static q2proto_error_t q2repro_download_data(q2proto_server_download_state_t *state, const uint8_t **data,
                                             size_t *remaining, size_t packet_remaining,
                                             q2proto_svc_download_t *svc_download)
{
    if (state->compress == Q2PROTO_DOWNLOAD_DATA_RAW_DEFLATE && packet_remaining > SVC_ZDOWNLOAD_SIZE) {
        size_t download_size = packet_remaining - SVC_ZDOWNLOAD_SIZE;
        download_size = MIN(download_size, *remaining);
        download_size = MIN(download_size, INT16_MAX);

        memset(svc_download, 0, sizeof(*svc_download));
        svc_download->compressed = true;
        svc_download->data = *data;
        svc_download->size = (int16_t)download_size;
        svc_download->uncompressed_size = -1; // indicates unknown compressed size

        *data += download_size;
        *remaining -= download_size;
        return q2proto_download_common_complete_struct(state, *remaining, svc_download);
    }

#if Q2PROTO_COMPRESSION_DEFLATE
    if (state->compress && packet_remaining > SVC_ZDOWNLOAD_SIZE) {
        size_t max_compressed = packet_remaining - SVC_ZDOWNLOAD_SIZE;
        max_compressed = MIN(max_compressed, INT16_MAX);

        if (!state->deflate_io_valid) {
            uintptr_t deflate_io;
            q2proto_error_t err =
                q2protoio_deflate_begin(state->deflate_args, max_compressed, Q2P_INFL_DEFL_RAW, &deflate_io);
            if (err != Q2P_ERR_SUCCESS)
                return err;
            state->deflate_io = deflate_io;
            state->deflate_io_valid = true;
        }

        size_t in_consumed = 0;
        CHECKED_IO(server_write, state->deflate_io,
                   q2protoio_write_raw(state->deflate_io, *data, *remaining, &in_consumed), "write download data");
        *data += in_consumed;
        *remaining -= in_consumed;

        const void *compressed_data;
        size_t compressed_size;
        q2proto_error_t err = q2protoio_deflate_get_data(state->deflate_io, NULL, &compressed_data, &compressed_size);
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
