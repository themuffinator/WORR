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

// Dummy q2protodbg_shownet_check, q2protodbg_shownet functions definition to make linking work

bool q2protodbg_shownet_check(uintptr_t io_arg, int level) { return false; }

void q2protodbg_shownet(uintptr_t io_arg, int level, int offset, const char *msg, ...) {}
