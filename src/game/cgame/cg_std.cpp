// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "q_std.h"

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
