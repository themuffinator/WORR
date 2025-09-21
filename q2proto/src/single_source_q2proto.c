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

// Use this for single source q2proto builds

#include "q2proto_client.c"
#include "q2proto_coords.c"
#include "q2proto_crc.c"
#include "q2proto_error.c"
#include "q2proto_internal_common.c"
#include "q2proto_internal_debug.c"
#include "q2proto_internal_download.c"
#include "q2proto_internal_fmt.c"
#include "q2proto_internal_maybe_zpacket.c"
#include "q2proto_internal_packing.c"
#include "q2proto_multicast.c"
#include "q2proto_proto_kex.c"
#include "q2proto_proto_q2pro.c"
#include "q2proto_proto_q2pro_extdemo.c"
#include "q2proto_proto_q2repro.c"
#include "q2proto_proto_r1q2.c"
#include "q2proto_proto_vanilla.c"
#include "q2proto_protocol.c"
#include "q2proto_server.c"
#include "q2proto_solid.c"
#include "q2proto_sound.c"
#include "q2proto_string.c"
