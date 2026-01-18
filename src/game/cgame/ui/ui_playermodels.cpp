#include "ui/ui_internal.h"

namespace ui {

static bool IconOfSkinExists(char *skin, char **pcxfiles, int npcxfiles)
{
    char scratch[MAX_QPATH];

    COM_StripExtension(scratch, skin, sizeof(scratch));
    Q_strlcat(scratch, "_i.pcx", sizeof(scratch));

    for (int i = 0; i < npcxfiles; i++) {
        if (Q_stricmp(pcxfiles[i], scratch) == 0)
            return true;
    }

    return false;
}

static int pmicmpfnc(const void *_a, const void *_b)
{
    const playerModelInfo_t *a = static_cast<const playerModelInfo_t *>(_a);
    const playerModelInfo_t *b = static_cast<const playerModelInfo_t *>(_b);

    if (Q_stricmp(a->directory, "male") == 0)
        return -1;
    if (Q_stricmp(b->directory, "male") == 0)
        return 1;

    if (Q_stricmp(a->directory, "female") == 0)
        return -1;
    if (Q_stricmp(b->directory, "female") == 0)
        return 1;

    return Q_stricmp(a->directory, b->directory);
}

void PlayerModel_Load(void)
{
    char scratch[MAX_QPATH];
    int ndirs = 0;
    char **dirnames = nullptr;

    Q_assert(!uis.numPlayerModels);

    if (!(dirnames = (char **)FS_ListFiles("players", NULL, FS_SEARCH_DIRSONLY, &ndirs)))
        return;

    for (int i = 0; i < ndirs; i++) {
        int npcxfiles = 0;
        char **pcxnames = nullptr;
        int nskins = 0;

        Q_concat(scratch, sizeof(scratch), "players/", dirnames[i], "/tris.md2");
        if (!FS_FileExists(scratch))
            continue;

        Q_concat(scratch, sizeof(scratch), "players/", dirnames[i]);
        pcxnames = (char **)FS_ListFiles(scratch, ".pcx", 0, &npcxfiles);
        if (!pcxnames)
            continue;

        for (int k = 0; k < npcxfiles; k++) {
            if (!strstr(pcxnames[k], "_i.pcx")) {
                if (IconOfSkinExists(pcxnames[k], pcxnames, npcxfiles))
                    nskins++;
            }
        }

        if (!nskins) {
            FS_FreeList((void **)pcxnames);
            continue;
        }

        char **skinnames = static_cast<char **>(UI_Malloc(sizeof(char *) * (nskins + 1)));
        skinnames[nskins] = NULL;

        for (int s = 0, k = 0; k < npcxfiles; k++) {
            if (!strstr(pcxnames[k], "_i.pcx")) {
                if (IconOfSkinExists(pcxnames[k], pcxnames, npcxfiles)) {
                    COM_StripExtension(scratch, pcxnames[k], sizeof(scratch));
                    skinnames[s++] = UI_CopyString(scratch);
                }
            }
        }

        FS_FreeList((void **)pcxnames);

        playerModelInfo_t *pmi = &uis.pmi[uis.numPlayerModels++];
        pmi->nskins = nskins;
        pmi->skindisplaynames = skinnames;
        pmi->directory = UI_CopyString(dirnames[i]);

        if (uis.numPlayerModels == MAX_PLAYERMODELS)
            break;
    }

    FS_FreeList((void **)dirnames);

    qsort(uis.pmi, uis.numPlayerModels, sizeof(uis.pmi[0]), pmicmpfnc);
}

void PlayerModel_Free(void)
{
    for (int i = 0; i < uis.numPlayerModels; i++) {
        playerModelInfo_t *pmi = &uis.pmi[i];
        if (pmi->skindisplaynames) {
            for (int j = 0; j < pmi->nskins; j++) {
                Z_Free(pmi->skindisplaynames[j]);
            }
            Z_Free(pmi->skindisplaynames);
        }
        Z_Free(pmi->directory);
        memset(pmi, 0, sizeof(*pmi));
    }

    uis.numPlayerModels = 0;
}

} // namespace ui
