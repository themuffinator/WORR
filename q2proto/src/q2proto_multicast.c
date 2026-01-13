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

q2proto_error_t q2proto_server_multicast_write(q2proto_multicast_protocol_t multicast_proto, uintptr_t io_arg,
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
        return q2proto_common_server_write_sound(multicast_proto, io_arg, &svc_message->sound);

    case Q2P_SVC_PRINT:
        return q2proto_common_server_write_print(io_arg, &svc_message->print);

    case Q2P_SVC_STUFFTEXT:
        return q2proto_common_server_write_stufftext(io_arg, &svc_message->stufftext);

    case Q2P_SVC_CONFIGSTRING:
        return q2proto_common_server_write_configstring(io_arg, &svc_message->configstring);

    default:
        break;
    }

    return Q2P_ERR_NOT_IMPLEMENTED;
}
