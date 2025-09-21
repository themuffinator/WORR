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
 * Limits (across all supported protocols)
 */
#ifndef Q2PROTO_LIMITS_H_
#define Q2PROTO_LIMITS_H_

/// Supported number of stats
#define Q2PROTO_STATS                 64
/// Supported number of damage indicators. Same value as in rerelease game.
#define Q2PROTO_MAX_DAMAGE_INDICATORS 4
/// Supported max number of entities
#define Q2PROTO_MAX_ENTITIES          8192
/// Supported number of localization print arguments
#define Q2PROTO_MAX_LOCALIZATION_ARGS 8

#endif // Q2PROTO_LIMITS_H_
