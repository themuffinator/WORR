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

#ifndef GAME3_PROXY_H_
#define GAME3_PROXY_H_

#define GAME3_API_VERSION_OLD    3       // game uses gclient_old_t, pmove_old_t
#define GAME3_API_VERSION_NEW    3302    // game uses gclient_new_t, pmove_new_t

extern const char *game_q2pro_restart_filesystem_ext;
extern const char *game_q2pro_customize_entity_ext;

game_export_t *GetGame3Proxy(game_import_t *import, void *game3_entry, void *game3_ex_entry);

#endif // GAME3_PROXY_H_
