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

#include "q2proto/q2proto.h"

// Dummy q2protoio_read_* function definitions to make linking work

uint8_t q2protoio_read_u8(uintptr_t io_arg) { return (uint8_t)-1; }

uint16_t q2protoio_read_u16(uintptr_t io_arg) { return (uint16_t)-1; }

uint32_t q2protoio_read_u32(uintptr_t io_arg) { return (uint32_t)-1; }

uint64_t q2protoio_read_u64(uintptr_t io_arg) { return (uint64_t)-1; }

q2proto_string_t q2protoio_read_string(uintptr_t io_arg)
{
    q2proto_string_t s = {.len = 0, .str = NULL};
    return s;
}

const void *q2protoio_read_raw(uintptr_t io_arg, size_t size, size_t *readcount) { return NULL; }

size_t q2protoio_read_available(uintptr_t io_arg) { return 0; }
