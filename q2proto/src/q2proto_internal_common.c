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
#include "q2proto_internal_common.h"

#include "q2proto_internal_defs.h"
#include "q2proto_internal_io.h"
#include "q2proto_internal_protocol.h"

#define READ_GAME_POSITION    read_short_coord
#define READ_SOUND_NAME       q2proto_common_client_read_sound_short
#define READ_TEMP_ENTITY_NAME q2proto_common_client_read_temp_entity_short

#include "q2proto_read_gamemsg.inc"

#undef READ_TEMP_ENTITY_NAME
#undef READ_SOUND_NAME
#undef READ_GAME_POSITION

#define READ_GAME_POSITION    read_float_coord
#define READ_SOUND_NAME       q2proto_common_client_read_sound_float
#define READ_TEMP_ENTITY_NAME q2proto_common_client_read_temp_entity_float

#include "q2proto_read_gamemsg.inc"

#undef READ_TEMP_ENTITY_NAME
#undef READ_SOUND_NAME
#undef READ_GAME_POSITION

q2proto_error_t q2proto_common_client_read_entity_bits(uintptr_t io_arg, uint64_t *bits, uint16_t *entnum)
{
    uint64_t total;
    READ_CHECKED(client_read, io_arg, total, u8);

    uint64_t b;
    if (total & U_MOREBITS1) {
        READ_CHECKED(client_read, io_arg, b, u8);
        total |= b << 8;
    }
    if (total & U_MOREBITS2) {
        READ_CHECKED(client_read, io_arg, b, u8);
        total |= b << 16;
    }
    if (total & U_MOREBITS3) {
        READ_CHECKED(client_read, io_arg, b, u8);
        total |= b << 24;
    }
    if (total & U_MOREBITS4) {
        READ_CHECKED(client_read, io_arg, b, u8);
        total |= b << 32;
    }

    if (total & U_NUMBER16)
        READ_CHECKED(client_read, io_arg, *entnum, u16);
    else
        READ_CHECKED(client_read, io_arg, *entnum, u8);

    *bits = total;

    return Q2P_ERR_SUCCESS;
}

int q2proto_common_entity_bits_size(uint64_t bits)
{
    int bits_size = 0;
    if (bits & U_MOREBITS4)
        bits_size = 5;
    else if (bits & U_MOREBITS3)
        bits_size = 4;
    else if (bits & U_MOREBITS2)
        bits_size = 3;
    else if (bits & U_MOREBITS1)
        bits_size = 2;
    else
        bits_size = 1;
    bits_size += (bits & U_NUMBER16) ? 2 : 1;
    return bits_size;
}

q2proto_error_t q2proto_common_server_write_entity_bits(uintptr_t io_arg, uint64_t bits, uint16_t entnum)
{
    if (entnum >= 256)
        bits |= U_NUMBER16;

    if (bits & 0xff00000000ull)
        bits |= U_MOREBITS4 | U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
    else if (bits & 0xff000000)
        bits |= U_MOREBITS3 | U_MOREBITS2 | U_MOREBITS1;
    else if (bits & 0x00ff0000)
        bits |= U_MOREBITS2 | U_MOREBITS1;
    else if (bits & 0x0000ff00)
        bits |= U_MOREBITS1;

    WRITE_CHECKED(server_write, io_arg, u8, bits & 0xff);
    if (bits & U_MOREBITS1)
        WRITE_CHECKED(server_write, io_arg, u8, (bits >> 8) & 0xff);
    if (bits & U_MOREBITS2)
        WRITE_CHECKED(server_write, io_arg, u8, (bits >> 16) & 0xff);
    if (bits & U_MOREBITS3)
        WRITE_CHECKED(server_write, io_arg, u8, (bits >> 24) & 0xff);
    if (bits & U_MOREBITS4)
        WRITE_CHECKED(server_write, io_arg, u8, (bits >> 32) & 0xff);

    if (bits & U_NUMBER16)
        WRITE_CHECKED(server_write, io_arg, u16, entnum);
    else
        WRITE_CHECKED(server_write, io_arg, u8, entnum);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_muzzleflash(uintptr_t io_arg, q2proto_svc_muzzleflash_t *muzzleflash,
                                                       uint16_t silenced_mask)
{
    READ_CHECKED(client_read, io_arg, muzzleflash->entity, i16);
    READ_CHECKED(client_read, io_arg, muzzleflash->weapon, u8);
    muzzleflash->silenced = muzzleflash->weapon & silenced_mask;
    muzzleflash->weapon &= ~silenced_mask;
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_layout(uintptr_t io_arg, q2proto_svc_layout_t *layout)
{
    READ_CHECKED(client_read, io_arg, layout->layout_str, string);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_inventory(uintptr_t io_arg, q2proto_svc_inventory_t *inventory)
{
    for (int i = 0; i < Q2PROTO_INVENTORY_ITEMS; i++) {
        READ_CHECKED(client_read, io_arg, inventory->inventory[i], i16);
    }

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_print(uintptr_t io_arg, q2proto_svc_print_t *print)
{
    READ_CHECKED(client_read, io_arg, print->level, u8);
    READ_CHECKED(client_read, io_arg, print->string, string);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_stufftext(uintptr_t io_arg, q2proto_svc_stufftext_t *stufftext)
{
    READ_CHECKED(client_read, io_arg, stufftext->string, string);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_configstring(uintptr_t io_arg, q2proto_svc_configstring_t *configstring)
{
    READ_CHECKED(client_read, io_arg, configstring->index, u16);
    READ_CHECKED(client_read, io_arg, configstring->value, string);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_centerprint(uintptr_t io_arg, q2proto_svc_centerprint_t *centerprint)
{
    READ_CHECKED(client_read, io_arg, centerprint->message, string);

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_read_download(uintptr_t io_arg, q2proto_svc_download_t *download)
{
    READ_CHECKED(client_read, io_arg, download->size, i16);
    READ_CHECKED(client_read, io_arg, download->percent, u8);
    if (download->size > 0)
        READ_CHECKED(client_read, io_arg, download->data, raw, download->size, NULL);
    return Q2P_ERR_SUCCESS;
}

#define NUMVERTEXNORMALS 162

static const q2proto_vec3_t bytedirs[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

q2proto_error_t q2proto_common_client_read_packed_direction(uintptr_t io_arg, float dir[3])
{
    uint8_t dir_idx;
    READ_CHECKED(client_read, io_arg, dir_idx, u8);
    if (dir_idx < 0 || dir_idx >= NUMVERTEXNORMALS)
        return Q2P_ERR_BAD_DATA;
    memcpy(dir, bytedirs[dir_idx], sizeof(q2proto_vec3_t));
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_layout(uintptr_t io_arg, const q2proto_svc_layout_t *layout)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_layout);
    WRITE_CHECKED(server_write, io_arg, string, &layout->layout_str);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_nop(uintptr_t io_arg)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_nop);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_disconnect(uintptr_t io_arg)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_disconnect);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_reconnect(uintptr_t io_arg)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_reconnect);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_sound(q2proto_multicast_protocol_t multicast_proto, uintptr_t io_arg,
                                                  const q2proto_svc_sound_t *sound)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_sound);
    WRITE_CHECKED(server_write, io_arg, u8, sound->flags);
    if (sound->flags & SND_Q2PRO_INDEX16)
        WRITE_CHECKED(server_write, io_arg, u16, sound->index);
    else
        WRITE_CHECKED(server_write, io_arg, u8, sound->index);

    if (sound->flags & SND_VOLUME)
        WRITE_CHECKED(server_write, io_arg, u8, sound->volume);
    if (sound->flags & SND_ATTENUATION)
        WRITE_CHECKED(server_write, io_arg, u8, sound->attenuation);
    if (sound->flags & SND_OFFSET)
        WRITE_CHECKED(server_write, io_arg, u8, sound->timeofs);

    if (sound->flags & SND_ENT)
        WRITE_CHECKED(server_write, io_arg, u16, (sound->entity << 3) | (sound->channel & 0x7));

    if (sound->flags & SND_POS) {
        float pos[3];
        q2proto_var_coords_get_float(&sound->pos, pos);
        q2proto_server_write_pos(multicast_proto, io_arg, pos);
    }

    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_print(uintptr_t io_arg, const q2proto_svc_print_t *print)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_print);
    WRITE_CHECKED(server_write, io_arg, u8, print->level);
    WRITE_CHECKED(server_write, io_arg, string, &print->string);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_stufftext(uintptr_t io_arg, const q2proto_svc_stufftext_t *stufftext)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_stufftext);
    WRITE_CHECKED(server_write, io_arg, string, &stufftext->string);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_configstring(uintptr_t io_arg,
                                                         const q2proto_svc_configstring_t *configstring)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_configstring);
    WRITE_CHECKED(server_write, io_arg, u16, configstring->index);
    WRITE_CHECKED(server_write, io_arg, string, &configstring->value);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_write_centerprint(uintptr_t io_arg, const q2proto_svc_centerprint_t *centerprint)
{
    WRITE_CHECKED(server_write, io_arg, u8, svc_centerprint);
    WRITE_CHECKED(server_write, io_arg, string, &centerprint->message);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_write_nop(uintptr_t io_arg)
{
    WRITE_CHECKED(client_write, io_arg, u8, clc_nop);
    return Q2P_ERR_SUCCESS;
}

static q2proto_error_t client_write_strcmd(uintptr_t io_arg, uint8_t cmd, const q2proto_string_t *str)
{
    WRITE_CHECKED(client_write, io_arg, u8, cmd);
    WRITE_CHECKED(client_write, io_arg, string, str);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_client_write_userinfo(uintptr_t io_arg, const q2proto_clc_userinfo_t *userinfo)
{
    return client_write_strcmd(io_arg, clc_userinfo, &userinfo->str);
}

q2proto_error_t q2proto_common_client_write_stringcmd(uintptr_t io_arg, const q2proto_clc_stringcmd_t *stringcmd)
{
    return client_write_strcmd(io_arg, clc_stringcmd, &stringcmd->cmd);
}

q2proto_error_t q2proto_common_server_read_userinfo(uintptr_t io_arg, q2proto_clc_userinfo_t *userinfo)
{
    READ_CHECKED(server_read, io_arg, userinfo->str, string);
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_common_server_read_stringcmd(uintptr_t io_arg, q2proto_clc_stringcmd_t *stringcmd)
{
    READ_CHECKED(server_read, io_arg, stringcmd->cmd, string);
    return Q2P_ERR_SUCCESS;
}
