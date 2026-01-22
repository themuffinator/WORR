#include "ui/ui_internal.h"

namespace ui {

static cvar_t *mapdb_episode;
static cvar_t *mapdb_level;
static cvar_t *mapdb_type;

static void MapDB_Run_f(void)
{
    const mapdb_t *mapdb = MapDB_Get();

    if (!strcmp(mapdb_type->string, "episode")) {
        int episode = mapdb_episode->integer;

        if (episode < 0 || episode >= mapdb->num_episodes) {
            Com_WPrintf("$cg_auto_e6ba1369ba86");
            return;
        }

        Cvar_Set("g_start_items", "");

        Cbuf_AddText(&cmd_buffer, mapdb->episodes[episode].command);
        Cbuf_AddText(&cmd_buffer, "\n");
    } else if (!strcmp(mapdb_type->string, "level")) {
        int level = mapdb_level->integer;

        if (level < 0 || level >= mapdb->num_maps) {
            Com_WPrintf("$cg_auto_bf69489cd5dd");
            return;
        }

        Cvar_Set("g_start_items", mapdb->maps[level].start_items);

        Cbuf_AddText(&cmd_buffer, "map ");
        Cbuf_AddText(&cmd_buffer, mapdb->maps[level].bsp);
        Cbuf_AddText(&cmd_buffer, "\n");
    } else {
        Com_WPrintf("$cg_auto_9064e9b262c3");
    }
}

void UI_MapDB_FetchEpisodes(char ***items, int *num_items)
{
    const mapdb_t *mapdb = MapDB_Get();

    *num_items = mapdb->num_episodes;
    *items = static_cast<char **>(UI_Mallocz(sizeof(char *) * (*num_items + 1)));
    for (int i = 0; i < *num_items; i++) {
        (*items)[i] = UI_CopyString(mapdb->episodes[i].name);
    }
}

void UI_MapDB_FetchUnits(char ***items, int **item_indices, int *num_items)
{
    const mapdb_t *mapdb = MapDB_Get();

    *num_items = 0;
    for (int i = 0; i < mapdb->num_maps; i++) {
        if (mapdb->maps[i].sp)
            (*num_items)++;
    }

    *items = static_cast<char **>(UI_Mallocz(sizeof(char *) * (*num_items + 1)));
    *item_indices = static_cast<int *>(UI_Mallocz(sizeof(int) * (*num_items)));

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

        (*items)[n] = UI_CopyString(va("(%s)\n%s",
                                      episode ? episode->name : "???",
                                      mapdb->maps[i].title));
        (*item_indices)[n] = i;
        n++;
    }
}

void UI_MapDB_Init()
{
    mapdb_episode = Cvar_Get("_mapdb_episode", "-1", 0);
    mapdb_level = Cvar_Get("_mapdb_level", "-1", 0);
    mapdb_type = Cvar_Get("_mapdb_type", "episode", 0);

    Cmd_AddCommand("_mapdb_run", MapDB_Run_f);
}

void UI_MapDB_Shutdown()
{
    Cmd_RemoveCommand("_mapdb_run");
}

} // namespace ui
