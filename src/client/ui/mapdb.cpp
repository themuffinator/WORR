/*
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

#include "ui.h"
#include "common/mapdb.h"

static cvar_t *mapdb_episode;
static cvar_t *mapdb_level;
static cvar_t *mapdb_type;

static void MapDB_Run_f(void)
{
	const mapdb_t *mapdb = MapDB_Get();

	if (!strcmp(mapdb_type->string, "episode")) {
		int episode = mapdb_episode->integer;

		if (episode < 0 || episode >= mapdb->num_episodes) {
			Com_WPrintf("bad _mapdb_episode\n");
			return;
		}

		Cvar_Set("g_start_items", "");

		Cbuf_AddText(&cmd_buffer, mapdb->episodes[episode].command);
		Cbuf_AddText(&cmd_buffer, "\n");
	} else if (!strcmp(mapdb_type->string, "level")) {
		int level = mapdb_level->integer;

		if (level < 0 || level >= mapdb->num_maps) {
			Com_WPrintf("bad _mapdb_level\n");
			return;
		}

		Cvar_Set("g_start_items", mapdb->maps[level].start_items);

		Cbuf_AddText(&cmd_buffer, "map ");
		Cbuf_AddText(&cmd_buffer, mapdb->maps[level].bsp);
		Cbuf_AddText(&cmd_buffer, "\n");
	} else {
		Com_WPrintf("unknown _mapdb_type\n");
	}
}

void UI_MapDB_FetchEpisodes(char ***items, int *num_items)
{
	const mapdb_t *mapdb = MapDB_Get();

	*num_items = mapdb->num_episodes;
	*items = UI_Mallocz(sizeof(char *) * (*num_items + 1));
    for (int i = 0; i < *num_items; i++) {
        (*items)[i] = UI_CopyString(mapdb->episodes[i].name);
    }
}

void UI_MapDB_FetchUnits(char ***items, int **item_indices, int *num_items)
{
	const mapdb_t *mapdb = MapDB_Get();

	*num_items = 0;

	for (int i = 0; i < mapdb->num_maps; i++)
		if (mapdb->maps[i].sp)
			(*num_items)++;

	*items = UI_Mallocz(sizeof(char *) * (*num_items + 1));
	*item_indices = UI_Mallocz(sizeof(int) * (*num_items));

	for (int i = 0, n = 0; i < mapdb->num_maps; i++) {
		if (!mapdb->maps[i].sp)
			continue;

		mapdb_episode_t *episode = NULL;

		for (int e = 0; e < mapdb->num_episodes; e++) {
			if (!strcmp(mapdb->episodes[e].id, mapdb->maps[i].episode)) {
				episode = &mapdb->episodes[e];
				break;
			}
		}

        (*items)[n] = UI_CopyString(va("(%s)\n%s", episode ? episode->name : "???", mapdb->maps[i].title));
		(*item_indices)[n] = i;
		n++;
	}
}

void UI_MapDB_Init(void)
{
	mapdb_episode = Cvar_Get("_mapdb_episode", "-1", 0);
	mapdb_level = Cvar_Get("_mapdb_level", "-1", 0);
	mapdb_type = Cvar_Get("_mapdb_type", "episode", 0);

	Cmd_AddCommand("_mapdb_run", MapDB_Run_f);
}

void UI_MapDB_Shutdown(void)
{
	Cmd_RemoveCommand("_mapdb_run");
}