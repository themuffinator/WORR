/*
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

/**\file
 * Misc. definitions
 */
#ifndef Q2PROTO_INTERNAL_DEFS_H_
#define Q2PROTO_INTERNAL_DEFS_H_

/**
 * \def MAYBE_UNUSED
 * To mark a function as possibly unused (to avoid compiler warnings)
 */
#if defined(__GNUC__)
    #define MAYBE_UNUSED __attribute__((unused))
#else
    #define MAYBE_UNUSED
#endif

/**\def MIN
 * Helper: Minimum of X and Y
 * \warning Evaluates \c X, \c Y multiple times!
 */
#define MIN(X, Y)          ((X) < (Y) ? (X) : (Y))
/**\def MAX
 * Helper: Maximum of X and Y
 * \warning Evaluates \c X, \c Y multiple times!
 */
#define MAX(X, Y)          ((X) > (Y) ? (X) : (Y))
/**\def CLAMP
 * Helper: clamp \c X between \c MIN and \c MAX.
 * \warning Evaluates \c X, \c MIN, \c MAX multiple times!
 */
#define CLAMP(X, MIN, MAX) ((X) < (MIN) ? (MIN) : ((X) > (MAX) ? (MAX) : (X)))

#define BIT(n)     (1U << (n))
#define BIT_ULL(n) (1ULL << (n))

#define SOUND_DEFAULT_VOLUME      255
#define SOUND_DEFAULT_ATTENUATION 64

#define _Q2PROTO_CONCAT2(X, Y) X##Y
#define _Q2PROTO_CONCAT(X, Y)  _Q2PROTO_CONCAT2(X, Y)

#endif // Q2PROTO_INTERNAL_DEFS_H_
