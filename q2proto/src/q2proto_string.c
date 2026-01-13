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
#include "q2proto/q2proto_string.h"

size_t q2pslcpy(char *dest, size_t dest_size, const q2proto_string_t *src)
{
    char *d = dest;
    const char *s = src->str;
    size_t dst_n = dest_size;
    size_t src_n = src->len;

    /* Copy as many bytes as will fit */
    if ((dst_n != 0) && (src_n != 0)) {
        while ((--dst_n != 0) && (src_n != 0)) {
            if ((*d++ = *s++) == '\0')
                break;
            src_n--;
        }
    }

    if ((dst_n == 0) || (src_n == 0)) {
        if (dest_size != 0)
            *d = '\0'; /* NUL-terminate dst */
    }

    return src->len;
}
