/*Copyright (c) 2024 ZeniMax Media Inc.
Licensed under the GNU General Public License 2.0.

q_std.cpp (Quake Standard Library Implementation) This file provides the implementation for the
custom standard library functions declared in `q_std.hpp`. It contains low-level utility
functions for string manipulation and parsing. Key Responsibilities: - String Parsing:
Implements `COM_Parse` and `COM_ParseEx`, the core functions used throughout the codebase to
parse tokenized strings from entity definitions, configuration files, and commands. - Safe
String Copying: Provides implementations for `Q_strlcpy` and `Q_strlcat`, which are safer
alternatives to the standard C library's `strcpy` and `strcat` because they prevent buffer
overflows by accepting the destination buffer size. - String Comparison: Implements
case-insensitive string comparison functions like `Q_strcasecmp` and `Q_strncasecmp`.*/

#include "../sgame/g_local.hpp"

//====================================================================================

g_fmt_data_t g_fmt_data;

static bool COM_IsSeparator(char c, const char* seps) {
	if (!c)
		return true;

	for (const char* sep = seps; *sep; sep++)
		if (*sep == c)
			return true;

	return false;
}

/*
==============
COM_ParseEx

Parse a token out of a string
==============
*/
char* COM_ParseEx(const char** data_p, const char* seps, char* buffer, size_t buffer_size, bool* truncated) {
	thread_local char	com_token[MAX_TOKEN_CHARS];
	bool			local_truncated = false;

	if (truncated)
		*truncated = false;

	if (!buffer) {
		buffer = com_token;
		buffer_size = MAX_TOKEN_CHARS;
	}

	if (!buffer_size)
		gi.Com_ErrorFmt("{}: called with zero buffer size.\n", __FUNCTION__);

	int			c;
	int			len;
	const char* data;

	data = *data_p;
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
				const size_t end_pos = std::min<size_t>(static_cast<size_t>(len), buffer_size - 1);
				buffer[end_pos] = '\0';

				if (local_truncated) {
					gi.Com_PrintFmt("Token exceeded {} chars, truncated.\n", buffer_size - 1);
					if (truncated)
						*truncated = true;
				}

				*data_p = data;
				return buffer;
			}
			if (static_cast<size_t>(len + 1) < buffer_size) {
				buffer[len] = c;
			}
			else {
				local_truncated = true;
			}
			len++;
		}
	}

	// parse a regular word
	do {
		if (static_cast<size_t>(len + 1) < buffer_size) {
			buffer[len] = c;
		}
		else {
			local_truncated = true;
		}
		len++;
		data++;
		c = *data;
	} while (!COM_IsSeparator(c, seps));

	const size_t end_pos = std::min<size_t>(static_cast<size_t>(len), buffer_size - 1);
	buffer[end_pos] = '\0';

	if (local_truncated) {
		gi.Com_PrintFmt("Token exceeded {} chars, truncated.\n", buffer_size - 1);
		if (truncated)
			*truncated = true;
	}

	*data_p = data;
	return buffer;
}
/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/
// NB: these funcs are duplicated in the engine; the DLL keeps its own copies.
int Q_strcasecmp(const char* s1, const char* s2) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

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

int Q_strncasecmp(const char* s1, const char* s2, size_t n) {
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

/*
=====================================================================

  BSD STRING UTILITIES - haleyjd 20170610

=====================================================================
*/
/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 /*
  * Copy src to string dst of size siz.  At most siz-1 characters
  * will be copied.  Always NUL terminates (unless siz == 0).
  * Returns strlen(src); if retval >= siz, truncation occurred.
  */
size_t Q_strlcpy(char* dst, const char* src, size_t siz) {
	char* d = dst;
	const char* s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0'; /* NUL-terminate dst */
		while (*s++)
			; // counter loop
	}

	return (s - src - 1); /* count does not include NUL */
}

/*
 * Appends src to string dst of size siz (unlike strncat, siz is the
 * full size of dst, not space left).  At most siz-1 characters
 * will be copied.  Always NUL terminates (unless siz == 0).
 * Returns strlen(src); if retval >= siz, truncation occurred.
 */
size_t Q_strlcat(char* dst, const char* src, size_t siz) {
	char* d = dst;
	const char* s = src;
	size_t n = siz;
	size_t dlen;

	/* FindEntity the end of dst and adjust bytes left but don't go past end */
	while (*d != '\0' && n-- != 0)
		d++;
	dlen = d - dst;
	n = siz - dlen;

	if (n == 0)
		return(dlen + strlen(s));
	while (*s != '\0') {
		if (n != 1) {
			*d++ = *s;
			n--;
		}
		s++;
	}
	*d = '\0';

	return (dlen + (s - src)); /* count does not include NUL */
}
//====================================================================
