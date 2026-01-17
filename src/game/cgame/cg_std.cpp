// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "cg_local.h"
#include "q_std.hpp"

static bool COM_IsSeparator(char c, const char *seps) {
	if (!c)
		return true;

	for (const char *sep = seps; *sep; sep++)
		if (*sep == c)
			return true;

	return false;
}

char *COM_ParseEx(const char **data_p, const char *seps, char *buffer, size_t buffer_size, bool *truncated)
{
	thread_local char com_token[MAX_TOKEN_CHARS];
	bool local_truncated = false;

	if (truncated)
		*truncated = false;

	if (!buffer) {
		buffer = com_token;
		buffer_size = MAX_TOKEN_CHARS;
	}

	if (!buffer_size)
		cgi.Com_Error("COM_ParseEx: called with zero buffer size.\n");

	int c;
	int len;
	const char *data = *data_p;

	len = 0;
	buffer[0] = '\0';

	if (!data) {
		*data_p = nullptr;
		return buffer;
	}

	// skip whitespace
skipwhite:
	while (COM_IsSeparator(c = *data, seps)) {
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
				const size_t end_pos =
					std::min<size_t>(static_cast<size_t>(len), buffer_size - 1);
				buffer[end_pos] = '\0';

				if (local_truncated) {
					cgi.Com_Print(G_Fmt("Token exceeded {} chars, truncated.\n",
					                    buffer_size - 1).data());
					if (truncated)
						*truncated = true;
				}

				*data_p = data;
				return buffer;
			}
			if (static_cast<size_t>(len + 1) < buffer_size) {
				buffer[len] = c;
			} else {
				local_truncated = true;
			}
			len++;
		}
	}

	// parse a regular word
	do {
		if (static_cast<size_t>(len + 1) < buffer_size) {
			buffer[len] = c;
		} else {
			local_truncated = true;
		}
		len++;
		data++;
		c = *data;
	} while (!COM_IsSeparator(c, seps));

	const size_t end_pos =
		std::min<size_t>(static_cast<size_t>(len), buffer_size - 1);
	buffer[end_pos] = '\0';

	if (local_truncated) {
		cgi.Com_Print(G_Fmt("Token exceeded {} chars, truncated.\n",
		                    buffer_size - 1).data());
		if (truncated)
			*truncated = true;
	}

	*data_p = data;
	return buffer;
}

int Q_strncasecmp(const char *s1, const char *s2, size_t n)
{
    int c1, c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--)
            return 0; // strings are equal until end point

        if (c1 != c2) {
            if (c1 >= 'a' && c1 <= 'z')
                c1 -= ('a' - 'A');
            if (c2 >= 'a' && c2 <= 'z')
                c2 -= ('a' - 'A');
            if (c1 != c2)
                return c1 < c2 ? -1 : 1; // strings not equal
        }
    } while (c1);

    return 0; // strings are equal
}

size_t Q_strlcpy(char *dst, const char *src, size_t siz)
{
    char *d = dst;
    const char *s = src;
    size_t n = siz;

    if (n != 0 && --n != 0) {
        do {
            if ((*d++ = *s++) == 0)
                break;
        } while (--n != 0);
    }

    if (n == 0) {
        if (siz != 0)
            *d = '\0'; // NUL-terminate dst
        while (*s++)
            ; // counter loop
    }

    return (s - src - 1); // count does not include NUL
}
