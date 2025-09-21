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

#define Q2PROTO_BUILD
#include "q2proto_internal_fmt.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define VA_STR_SIZE 256
#define NUM_VA_STR  8

static _Thread_local int va_index;
static _Thread_local char va_str[NUM_VA_STR][VA_STR_SIZE];

const char *q2proto_va(const char *fmt, ...)
{
    char *s = va_str[va_index];
    va_index = (va_index + 1) % NUM_VA_STR;

    va_list argptr;

    va_start(argptr, fmt);
    vsnprintf(s, VA_STR_SIZE, fmt, argptr);
    va_end(argptr);

    return s;
}

int q2proto_snprintf_update(char **buf, size_t *buf_size, const char *format, ...)
{
    int result;
    va_list argptr;

    va_start(argptr, format);
    result = vsnprintf(*buf, *buf_size, format, argptr);
    va_end(argptr);

    size_t fmt_size = *buf ? strlen(*buf) : 0;
    *buf += fmt_size;
    *buf_size -= fmt_size;
    return result;
}
