/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2024 Jonathan "Paril" Barkley

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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include "shared/shared.h"
#include "system/system.h"

#include <stdbool.h>
#include <stddef.h>

// Locate path of Steam installation (platform-specific)
bool Steam_GetInstallationPath(char *out_dir, size_t out_dir_length);
// Locate Quake 2 Steam install
bool Steam_FindQuake2Path(rerelease_mode_t rr_mode, char *out_dir, size_t out_dir_length);

#ifdef __cplusplus
}
#endif

