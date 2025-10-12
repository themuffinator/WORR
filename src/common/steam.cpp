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

#include "shared/shared.h"
#include "common/files.h"
#include "common/steam.h"
#include "common/zone.h"

#define COM_ParseExpect(d, s) \
    !strcmp(COM_Parse((d)), (s))

static void skip_vdf_value(const char **file_contents)
{
    char *value = COM_Parse(file_contents);

    if (!strcmp(value, "{")) {
        while (true) {
            COM_Parse(file_contents);
            skip_vdf_value(file_contents);
        }
    }
}

static bool parse_vdf_apps_list(const char **file_contents, const char *app_id)
{
    if (!COM_ParseExpect(file_contents, "{")) {
        return false;
    }

    bool game_found = false;

    while (true) {
        char *key = COM_Parse(file_contents);

        if (!*key || !strcmp(key, "}")) {
            return game_found;
        }

        COM_Parse(file_contents);

        if (!strcmp(key, app_id)) {
            game_found = true;
        }
    }

    return game_found;
}

static bool parse_library_vdf(const char **file_contents, const char *app_id, char *out_dir, size_t out_dir_length)
{
    char library_path[MAX_OSPATH];

    while (true) {
        char *key = COM_Parse(file_contents);

        if (!*key || !strcmp(key, "}")) {
            return false;
        } else if (!strcmp(key, "path")) {
            COM_ParseToken(file_contents, library_path, sizeof(library_path), PARSE_FLAG_ESCAPE);
        } else if (!strcmp(key, "apps")) {
            if (parse_vdf_apps_list(file_contents, app_id)) {
                Q_strlcpy(out_dir, library_path, out_dir_length);
                return true;
            }
        } else {
            skip_vdf_value(file_contents);
        }
    }

    return false;
}

static bool parse_vdf_libraryfolders(const char **file_contents, const char *app_id, char *out_dir, size_t out_dir_length)
{
    // parse library folders VDF
    if (!COM_ParseExpect(file_contents, "libraryfolders") ||
        !COM_ParseExpect(file_contents, "{")) {
        return false;
    }

    while (true) {
        char *token = COM_Parse(file_contents);

        // done with folders
        if (!*token || !strcmp(token, "}")) {
            break;
        }

        // should be an integer; check the entrance of the folder
        if (!COM_ParseExpect(file_contents, "{")) {
            break;
        }

        if (parse_library_vdf(file_contents, app_id, out_dir, out_dir_length)) {
            return true;
        }
    }

    return false;
}

static bool find_steam_app_path(const char *app_id, char *out_dir, size_t out_dir_length)
{
    bool result = false;

    // grab Steam installation path
    char folder_path[MAX_OSPATH];

    if (!Steam_GetInstallationPath(folder_path, sizeof(folder_path)))
        return false;

    // grab library folders file
    Q_strlcat(folder_path, "/steamapps/libraryfolders.vdf", sizeof(folder_path));

    FILE *libraryfolders = fopen(folder_path, "rb");

    if (!libraryfolders)
        return result;

    fseek(libraryfolders, 0, SEEK_END);
    long len = ftell(libraryfolders);
    fseek(libraryfolders, 0, SEEK_SET);

    char *file_contents = Z_Malloc(len + 1);
    file_contents[len] = '\0';

    size_t file_read = fread((void *) file_contents, 1, len, libraryfolders);

    fclose(libraryfolders);

    if (file_read != len) {
        Com_EPrintf("Error reading libraryfolders.vdf.\n");
        result = false;
        goto exit;
    }

    char *parse_contents = file_contents;

    result = parse_vdf_libraryfolders((const char **) &parse_contents, app_id, out_dir, out_dir_length);

exit:
    Z_Free(file_contents);
    return result;
}

#define QUAKE_II_STEAM_APP_ID           "2320"

bool Steam_FindQuake2Path(rerelease_mode_t rr_mode, char *out_dir, size_t out_dir_length)
{
    if (!find_steam_app_path(QUAKE_II_STEAM_APP_ID, out_dir, out_dir_length))
        return false;

    Q_strlcat(out_dir, "/steamapps/common/Quake 2", out_dir_length);

    // found Steam dir - see if the mode we want is available
    listfiles_t list = {
        .flags = FS_SEARCH_DIRSONLY,
        .baselen = strlen(out_dir) + 1,
    };

    Sys_ListFiles_r(&list, out_dir, 0);

    bool has_rerelease = false;

    for (int i = 0; i < list.count; i++) {
        char *s = list.files[i];

        if (!Q_stricmp(s, "rerelease")) {
            has_rerelease = true;
        }

        Z_Free(s);
    }

    Z_Free(list.files);

    if (rr_mode == RERELEASE_MODE_YES && has_rerelease) {
        Q_strlcat(out_dir, PATH_SEP_STRING "rerelease", out_dir_length);
    }

    return true;
}
