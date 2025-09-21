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
 * Internal helpers for q2proto_string_t
 */
#ifndef Q2PROTO_INTERNAL_STRING_H_
#define Q2PROTO_INTERNAL_STRING_H_

#include "q2proto/q2proto_string.h"

#if defined(_MSC_VER) || defined(__MINGW32__)
    #include <malloc.h>
    #if !defined(alloca)
        #define alloca _alloca
    #endif
#elif !defined(__FreeBSD__) && !defined(__OpenBSD__)
    #include <alloca.h>
#endif

#include <errno.h>
#include <stdlib.h>

static const q2proto_string_t empty_q2proto_string = {};

static inline q2proto_string_t q2ps_substr(const q2proto_string_t *str, size_t offset)
{
    offset = offset > str->len ? str->len : offset;
    q2proto_string_t result = {.str = str->str + offset, .len = str->len - offset};
    return result;
}

// strchr() for q2proto_string_t
static inline const char *q2pschr(const q2proto_string_t *str, char ch)
{
    const char *p = str->str;
    const char *end = p + str->len;
    while (p < end) {
        if (*p == ch)
            return p;
        ++p;
    }
    return NULL;
}

// strtol() for q2proto_string_t
static inline long q2pstol(const q2proto_string_t *str, int base)
{
    char *buf = alloca(str->len + 1);
    memcpy(buf, str->str, str->len);
    buf[str->len] = 0;

    char *endp;
    long result = strtol(buf, &endp, base);
    // detect empty input, invalid digits
    if (endp == buf || *endp != 0)
        errno = EINVAL;
    return result;
}

static inline bool next_token(q2proto_string_t *token, q2proto_string_t *str, char sep)
{
    if (str->len == 0)
        return false;

    const char *token_end = q2pschr(str, sep);
    if (token_end == NULL) {
        *token = *str;
        *str = empty_q2proto_string;
    } else {
        token->str = str->str;
        token->len = token_end - token->str;
        const char *str_end = str->str + str->len;
        str->str = token_end + 1;
        str->len = str_end - str->str;
    }
    return true;
}

#endif // Q2PROTO_INTERNAL_STRING_H_
