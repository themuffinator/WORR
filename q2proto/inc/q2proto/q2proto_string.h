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

/**\file
 * String type
 */
#ifndef Q2PROTO_STRING_H_
#define Q2PROTO_STRING_H_

#include "q2proto_defs.h"
#include <string.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**\name Strings
 * @{ */
/// String representation. \warning Do \b not assume string data to be \b null-terminated!
typedef struct q2proto_string_s {
    /// Pointer to start of string data
    const char *str;
    /// String length
    size_t len;
} q2proto_string_t;

/// Helper: Create a q2proto_string_t from a null-terminated string
static inline q2proto_string_t q2proto_make_string(const char *s)
{
    q2proto_string_t q2p_str = {.str = s, .len = s ? strlen(s) : 0};
    return q2p_str;
}

/// Helper: Copy a q2proto_string_t into a character buffer
Q2PROTO_PUBLIC_API size_t q2pslcpy(char *dest, size_t dest_size, const q2proto_string_t *src);
/** @} */

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_STRING_H_
