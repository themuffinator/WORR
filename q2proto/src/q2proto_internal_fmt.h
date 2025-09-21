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
 * Formatting helpers
 */
#ifndef Q2PROTO_INTERNAL_FMT_H_
#define Q2PROTO_INTERNAL_FMT_H_

#include "q2proto/q2proto_defs.h"
#include <stddef.h>

/**\name Formatting helpers
 * @{ */
/// Helper: Format into a string with static lifetime
Q2PROTO_PRIVATE_API const char *q2proto_va(const char *fmt, ...);

/**
 * snprintf() to a buffer, and update pointer and remaining size.
 * \returns Return value of snprintf().
 */
Q2PROTO_PRIVATE_API int q2proto_snprintf_update(char **buf, size_t *buf_size, const char *format, ...);
/** @} */

#endif // Q2PROTO_INTERNAL_FMT_H_
