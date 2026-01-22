/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "shared/shared.h"
#include "common/common.h"
#include "common/utils.h"
#include "common/zone.h"

#if USE_CLIENT

#define QCHAR_BOX   11

#include "unicode_translit.h"

/*
==================
UTF8_ReadCodePoint

Reads at most 4 bytes from *src and advances the pointer.
Returns 32-bit codepoint, or UNICODE_UNKNOWN on error.
==================
*/
uint32_t UTF8_ReadCodePoint(const char **src)
{
    static const uint32_t mincode[3] = { 0x80, 0x800, 0x10000 };
    const char  *text = *src;
    uint32_t    code;
    uint8_t     first, cont;
    int         bytes, i;

    first = text[0];
    if (!first)
        return 0;

    if (first < 128) {
        *src = text + 1;
        return first;
    }

    bytes = 7 - Q_log2(first ^ 255);
    if (bytes < 2 || bytes > 4) {
        *src = text + 1;
        return UNICODE_UNKNOWN;
    }

    code = first & (127 >> bytes);
    for (i = 1; i < bytes; i++) {
        cont = text[i];
        if ((cont & 0xC0) != 0x80) {
            *src = text + i;
            return UNICODE_UNKNOWN;
        }
        code = (code << 6) | (cont & 63);
    }

    *src = text + i;

    if (code > UNICODE_MAX)
        return UNICODE_UNKNOWN; // out of range

    if (code >= 0xD800 && code <= 0xDFFF)
        return UNICODE_UNKNOWN; // surrogate

    if (code < mincode[bytes - 2])
        return UNICODE_UNKNOWN; // overlong

    return code;
}

size_t UTF8_EncodeCodePoint(uint32_t codepoint, char *dst, size_t size)
{
    if (!dst || !size)
        return 0;

    if (codepoint <= 0x7F) {
        dst[0] = (char)codepoint;
        return 1;
    }

    if (codepoint <= 0x7FF) {
        if (size < 2)
            return 0;
        dst[0] = (char)(0xC0 | ((codepoint >> 6) & 0x1F));
        dst[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    }

    if (codepoint <= 0xFFFF) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            return 0;
        if (size < 3)
            return 0;
        dst[0] = (char)(0xE0 | ((codepoint >> 12) & 0x0F));
        dst[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        dst[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }

    if (codepoint <= UNICODE_MAX) {
        if (size < 4)
            return 0;
        dst[0] = (char)(0xF0 | ((codepoint >> 18) & 0x07));
        dst[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        dst[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        dst[3] = (char)(0x80 | (codepoint & 0x3F));
        return 4;
    }

    return 0;
}

size_t UTF8_CountChars(const char *text, size_t bytes)
{
    size_t count = 0;
    const char *ptr = text;
    const char *end = text ? text + bytes : NULL;

    if (!text)
        return 0;

    while (ptr < end && *ptr) {
        const char *next = ptr;
        uint32_t code = UTF8_ReadCodePoint(&next);
        if (!code)
            break;
        if (next > end)
            break;
        ptr = next;
        count++;
    }

    return count;
}

size_t UTF8_OffsetForChars(const char *text, size_t chars)
{
    size_t offset = 0;
    const char *ptr = text;

    if (!text)
        return 0;

    for (size_t i = 0; i < chars && *ptr; i++) {
        const char *next = ptr;
        uint32_t code = UTF8_ReadCodePoint(&next);
        if (!code)
            break;
        offset += (size_t)(next - ptr);
        ptr = next;
    }

    return offset;
}

static const char *UTF8_TranslitCode(uint32_t code)
{
    int left = 0;
    int right = q_countof(unicode_translit) - 1;

    if (code > unicode_translit[right].code)
        return NULL;

    while (left <= right) {
        int i = (left + right) / 2;
        if (unicode_translit[i].code < code)
            left = i + 1;
        else if (unicode_translit[i].code > code)
            right = i - 1;
        else
            return unicode_translit[i].remap;
    }

    return NULL;
}

/*
==================
UTF8_TranslitBuffer

Transliterates a string from UTF-8 to Quake encoding.

Returns the number of characters (not including the NUL terminator) that would
be written into output buffer. Return value >= size signifies overflow.
==================
*/
size_t UTF8_TranslitBuffer(char *dst, const char *src, size_t size)
{
    size_t len = 0;

    while (*src) {
        // ASCII fast path
        uint8_t c = *src;
        if (q_likely(c < 128)) {
            if (++len < size)
                *dst++ = c;
            src++;
            continue;
        }

        // a codepoint produces from 1 to 4 Quake characters
        const char *res = UTF8_TranslitCode(UTF8_ReadCodePoint(&src));
        if (res) {
            for (int i = 0; i < 4 && res[i]; i++)
                if (++len < size)
                    *dst++ = res[i];
        } else {
            if (++len < size)
                *dst++ = QCHAR_BOX;
        }
    }

    if (size)
        *dst = 0;

    return len;
}

/*
==================
UTF8_TranslitString

Transliterates a string from UTF-8 to Quake encoding.
Allocates copy of the string. Returned data must be Z_Free'd.
==================
*/
char *UTF8_TranslitString(const char *src)
{
    size_t len = UTF8_TranslitBuffer(NULL, src, 0) + 1;
    char *copy = Z_Malloc(len);
    UTF8_TranslitBuffer(copy, src, len);
    return copy;
}

#endif // USE_CLIENT
