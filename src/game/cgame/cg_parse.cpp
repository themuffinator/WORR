// Licensed under the GNU General Public License 2.0.

#include "cg_local.h"

static bool CG_IsSeparator(char c, const char *seps)
{
    if (!c)
        return true;

    for (const char *sep = seps; *sep; sep++) {
        if (*sep == c)
            return true;
    }

    return false;
}

/*
==============
COM_ParseEx

Parse a token out of a string
==============
*/
char *COM_ParseEx(const char **data_p, const char *seps, char *buffer, size_t buffer_size)
{
    static char com_token[MAX_TOKEN_CHARS];

    if (!buffer) {
        buffer = com_token;
        buffer_size = MAX_TOKEN_CHARS;
    }

    int c;
    int len;
    const char *data;

    data = *data_p;
    len = 0;
    buffer[0] = '\0';

    if (!data) {
        *data_p = nullptr;
        return buffer;
    }

    // skip whitespace
skipwhite:
    while (CG_IsSeparator(c = *data, seps)) {
        if (c == '\0') {
            *data_p = nullptr;
            return buffer;
        }
        data++;
    }

    // skip // comments
    if (c == '/' && data[1] == '/') {
        while (*data && *data != '\n')
            data++;
        goto skipwhite;
    }

    // handle quoted strings specially
    if (c == '\"') {
        data++;
        while (1) {
            c = *data++;
            if (c == '\"' || !c) {
                const size_t endpos = std::min<size_t>(len, buffer_size - 1);
                buffer[endpos] = '\0';
                *data_p = data;
                return buffer;
            }
            if (len < (int)buffer_size) {
                buffer[len] = c;
                len++;
            }
        }
    }

    // parse a regular word
    do {
        if (len < (int)buffer_size) {
            buffer[len] = c;
            len++;
        }
        data++;
        c = *data;
    } while (!CG_IsSeparator(c, seps));

    if (len == (int)buffer_size) {
        cgi.Com_Print(G_Fmt("Token exceeded {} chars, discarded.\n", buffer_size).data());
        len = 0;
    }
    buffer[len] = '\0';

    *data_p = data;
    return buffer;
}
