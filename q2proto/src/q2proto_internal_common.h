/*
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

/**\file
 * Common protocol functions
 */
#ifndef Q2PROTO_INTERNAL_COMMON_H_
#define Q2PROTO_INTERNAL_COMMON_H_

#include "q2proto/q2proto.h"

/**\name Common protocol functions
 * @{ */
/**
 * Helper to select the right flags from the common pattern U_FOO8, U_FOO16, U_FOO32 (which is U_FOO8 | U_FOO16).
 * \param value Value to compute width of.
 * \param flag8 Flag for 8-bit wide value.
 * \param flag16 Flag for 16-bit wide value.
 * \param uint16_safe Whether unsigned 16-bit values are safe.
 * \returns Width flags.
 */
static inline uint64_t q2proto_common_choose_width_flags(uint32_t value, uint64_t flag8, uint64_t flag16,
                                                         bool uint16_safe)
{
    uint32_t mask32 = uint16_safe ? 0xffff0000 : 0xffff8000; // don't confuse old clients
    if (value & mask32)
        return flag8 | flag16;
    else if (value & 0xff00)
        return flag16;
    else
        return flag8;
}

Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_entity_bits(uintptr_t io_arg, uint64_t *bits,
                                                                           uint16_t *entnum);
/// Debug helper: Return number of bytes occupied by given entity bits
Q2PROTO_PRIVATE_API int q2proto_common_entity_bits_size(uint64_t bits);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_entity_bits(uintptr_t io_arg, uint64_t bits,
                                                                            uint16_t entnum);

Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_muzzleflash(uintptr_t io_arg,
                                                                           q2proto_svc_muzzleflash_t *muzzleflash,
                                                                           uint16_t silenced_mask);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_temp_entity_short(
    uintptr_t io_arg, q2proto_game_api_t game_api, q2proto_svc_temp_entity_t *temp_entity);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_temp_entity_float(
    uintptr_t io_arg, q2proto_game_api_t game_api, q2proto_svc_temp_entity_t *temp_entity);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_layout(uintptr_t io_arg, q2proto_svc_layout_t *layout);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_inventory(uintptr_t io_arg,
                                                                         q2proto_svc_inventory_t *inventory);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_sound_short(uintptr_t io_arg,
                                                                           q2proto_svc_sound_t *sound);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_sound_float(uintptr_t io_arg,
                                                                           q2proto_svc_sound_t *sound);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_print(uintptr_t io_arg, q2proto_svc_print_t *print);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_stufftext(uintptr_t io_arg,
                                                                         q2proto_svc_stufftext_t *stufftext);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_configstring(uintptr_t io_arg,
                                                                            q2proto_svc_configstring_t *configstring);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_centerprint(uintptr_t io_arg,
                                                                           q2proto_svc_centerprint_t *centerprint);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_download(uintptr_t io_arg,
                                                                        q2proto_svc_download_t *download);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_read_packed_direction(uintptr_t io_arg, float dir[3]);

Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_layout(uintptr_t io_arg,
                                                                       const q2proto_svc_layout_t *layout);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_nop(uintptr_t io_arg);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_disconnect(uintptr_t io_arg);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_reconnect(uintptr_t io_arg);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_sound(q2proto_multicast_protocol_t multicast_proto,
                                                                      uintptr_t io_arg,
                                                                      const q2proto_svc_sound_t *sound);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_print(uintptr_t io_arg,
                                                                      const q2proto_svc_print_t *print);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_write_stufftext(uintptr_t io_arg,
                                                                          const q2proto_svc_stufftext_t *stufftext);
Q2PROTO_PRIVATE_API q2proto_error_t
q2proto_common_server_write_configstring(uintptr_t io_arg, const q2proto_svc_configstring_t *configstring);
Q2PROTO_PRIVATE_API q2proto_error_t
q2proto_common_server_write_centerprint(uintptr_t io_arg, const q2proto_svc_centerprint_t *centerprint);

Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_write_nop(uintptr_t io_arg);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_write_userinfo(uintptr_t io_arg,
                                                                         const q2proto_clc_userinfo_t *userinfo);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_client_write_stringcmd(uintptr_t io_arg,
                                                                          const q2proto_clc_stringcmd_t *stringcmd);

Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_read_userinfo(uintptr_t io_arg,
                                                                        q2proto_clc_userinfo_t *userinfo);
Q2PROTO_PRIVATE_API q2proto_error_t q2proto_common_server_read_stringcmd(uintptr_t io_arg,
                                                                         q2proto_clc_stringcmd_t *stringcmd);
/** @} */

#endif // Q2PROTO_INTERNAL_COMMON_H_
