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

// Dummy q2protoio_write_* function definitions to make linking work

void q2protoio_write_u8(uintptr_t io_arg, uint8_t x) {}
void q2protoio_write_u16(uintptr_t io_arg, uint16_t x) {}
void q2protoio_write_u32(uintptr_t io_arg, uint32_t x) {}
void q2protoio_write_u64(uintptr_t io_arg, uint64_t x) {}
void *q2protoio_write_reserve_raw(uintptr_t io_arg, size_t size) { return NULL; }
void q2protoio_write_raw(uintptr_t io_arg, const void *data, size_t size, size_t *written)
{
    if (written)
        *written = 0;
}
size_t q2protoio_write_available(uintptr_t io_arg) { return 0; }
