#include "ui/ui_internal.h"

#include "common/files.h"
#include "common/mdfour.h"

namespace ui {

#define DEMO_EXTENSIONS ".dm2;.dm2.gz;.mvd2;.mvd2.gz"
#define DEMO_EXTRASIZE offsetof(demoEntry_t, name)
#define DEMO_MVD_POV "\x90\xcd\xd6\xc4\x91"
#define DEMO_DIR_SIZE "\x90\xc4\xc9\xd2\x91"

enum demoEntryType {
    ENTRY_UP = 1,
    ENTRY_DN,
    ENTRY_DEMO
};

enum demoColumn {
    COL_NAME = 0,
    COL_DATE,
    COL_SIZE,
    COL_MAP,
    COL_POV,
    COL_MAX
};

struct demoEntry_t {
    unsigned type;
    int64_t size;
    time_t mtime;
    char name[1];
};

class DemoBrowserPage : public MenuPage {
public:
    DemoBrowserPage();
    ~DemoBrowserPage() override { FreeList(); }
    const char *Name() const override { return "demos"; }
    void OnOpen() override;
    void OnClose() override;
    void Draw() override;
    Sound KeyEvent(int key) override;
    void MouseEvent(int x, int y, bool down) override;
    void UpdateSortFromCvar();

private:
    void BuildList();
    void FreeList();
    void BuildName(const file_info_t *info, char **cache);
    void BuildDir(const char *name, int type);
    void CalcHash(void **list);
    char *LoadCache();
    void WriteCache();
    Sound Activate();
    Sound Change();
    Sound Sort();
    Sound EnterDirectory(demoEntry_t *e);
    Sound LeaveDirectory();
    Sound PlayDemo(demoEntry_t *e);
    bool FileNameOk(const char *s) const;
    void UpdateLayout();

    ListWidget list_;
    int numDirs_ = 0;
    uint8_t hash_[16]{};
    char browse_[MAX_OSPATH]{};
    int selection_ = 0;
    int year_ = 0;
    int widest_map_ = 3;
    int widest_pov_ = 3;
    uint64_t total_bytes_ = 0;
    char status_[32]{};
    const char *title_ = "Demo Browser";

    cvar_t *ui_sortdemos_ = nullptr;
    cvar_t *ui_listalldemos_ = nullptr;
};

static DemoBrowserPage *g_demo_page;

static void ui_sortdemos_changed(cvar_t *self)
{
    (void)self;
    if (g_demo_page)
        g_demo_page->UpdateSortFromCvar();
}

DemoBrowserPage::DemoBrowserPage()
{
    ui_sortdemos_ = Cvar_Get("ui_sortdemos", "1", 0);
    ui_listalldemos_ = Cvar_Get("ui_listalldemos", "0", 0);
    ui_sortdemos_->changed = ui_sortdemos_changed;

    list_.mlFlags = MLF_HEADER | MLF_SCROLLBAR;
    list_.columns.resize(COL_MAX);
    list_.columns[COL_NAME].name = browse_;
    list_.columns[COL_NAME].uiFlags = UI_LEFT;
    list_.columns[COL_DATE].name = "Date";
    list_.columns[COL_DATE].uiFlags = UI_CENTER;
    list_.columns[COL_SIZE].name = "Size";
    list_.columns[COL_SIZE].uiFlags = UI_RIGHT;
    list_.columns[COL_MAP].name = "Map";
    list_.columns[COL_MAP].uiFlags = UI_CENTER;
    list_.columns[COL_POV].name = "POV";
    list_.columns[COL_POV].uiFlags = UI_CENTER;

    list_.onActivate = [this](ListWidget *) { return Activate(); };
    list_.onChange = [this](ListWidget *) { return Change(); };
    list_.onSort = [this](ListWidget *) { return Sort(); };

    Q_strlcpy(browse_, "/demos", sizeof(browse_));
    g_demo_page = this;
}

void DemoBrowserPage::UpdateSortFromCvar()
{
    int i = Cvar_ClampInteger(ui_sortdemos_, -COL_MAX, COL_MAX);
    if (i > 0) {
        list_.sortdir = 1;
        list_.sortcol = i - 1;
    } else if (i < 0) {
        list_.sortdir = -1;
        list_.sortcol = -i - 1;
    } else {
        list_.sortdir = 0;
        list_.sortcol = 0;
    }

    if (!list_.items.empty() && list_.sortdir)
        Sort();
}

void DemoBrowserPage::BuildName(const file_info_t *info, char **cache)
{
    char buffer[MAX_OSPATH];
    char date[MAX_QPATH];
    demoInfo_t demo{};
    demoEntry_t *e;
    struct tm *tm;
    size_t len;

    Q_strlcpy(demo.map, "???", sizeof(demo.map));
    Q_strlcpy(demo.pov, "???", sizeof(demo.pov));

    if (cache) {
        char *s = *cache;
        char *p = strchr(s, '\\');
        if (p) {
            *p = 0;
            Q_strlcpy(demo.map, s, sizeof(demo.map));
            s = p + 1;
            p = strchr(s, '\\');
            if (p) {
                *p = 0;
                Q_strlcpy(demo.pov, s, sizeof(demo.pov));
                s = p + 1;
            }
        }
        *cache = s;
    } else {
        Q_concat(buffer, sizeof(buffer), browse_, "/", info->name);
        CL_GetDemoInfo(buffer, &demo);
        if (demo.mvd)
            Q_strlcpy(demo.pov, DEMO_MVD_POV, sizeof(demo.pov));
    }

    len = strlen(demo.map);
    if (len > 8)
        len = 8;
    widest_map_ = max(widest_map_, static_cast<int>(len));

    len = strlen(demo.pov);
    widest_pov_ = max(widest_pov_, static_cast<int>(len));

    len = 0;
    time_t mtime = info->mtime;
    if ((tm = localtime(&mtime)) != NULL) {
        if (tm->tm_year == year_) {
            len = strftime(date, sizeof(date), "%b %d %H:%M", tm);
        } else {
            len = strftime(date, sizeof(date), "%b %d  %Y", tm);
        }
    }
    if (!len)
        Q_strlcpy(date, "???", sizeof(date));

    Com_FormatSize(buffer, sizeof(buffer), info->size);

    e = static_cast<demoEntry_t *>(UI_FormatColumns(
        DEMO_EXTRASIZE, info->name, date, buffer, demo.map, demo.pov, NULL));
    e->type = ENTRY_DEMO;
    e->size = info->size;
    e->mtime = info->mtime;

    total_bytes_ += info->size;
    list_.items.push_back(e);
    list_.numItems = static_cast<int>(list_.items.size());
}

void DemoBrowserPage::BuildDir(const char *name, int type)
{
    demoEntry_t *e = static_cast<demoEntry_t *>(
        UI_FormatColumns(DEMO_EXTRASIZE, name, "-", DEMO_DIR_SIZE, "-", "-", NULL));
    e->type = type;
    e->size = 0;
    e->mtime = 0;

    list_.items.push_back(e);
    list_.numItems = static_cast<int>(list_.items.size());
}

char *DemoBrowserPage::LoadCache()
{
    char buffer[MAX_OSPATH];
    char *cache = nullptr;
    int len = 0;
    uint8_t hash[16]{};

    if (Q_concat(buffer, sizeof(buffer), browse_, "/" COM_DEMOCACHE_NAME) >= sizeof(buffer))
        return nullptr;

    len = FS_LoadFileEx(buffer, (void **)&cache, FS_TYPE_REAL | FS_PATH_GAME | FS_DIR_HOME, TAG_FILESYSTEM);
    if (!cache)
        return nullptr;

    if (len < 33)
        goto fail;

    for (int i = 0; i < 16; i++) {
        int c1 = Q_charhex(cache[i * 2 + 0]);
        int c2 = Q_charhex(cache[i * 2 + 1]);
        if (c1 == -1 || c2 == -1)
            goto fail;
        hash[i] = (c1 << 4) | c2;
    }

    if (cache[32] != '\\')
        goto fail;

    if (memcmp(hash, hash_, 16))
        goto fail;

    return cache;

fail:
    FS_FreeFile(cache);
    return nullptr;
}

void DemoBrowserPage::WriteCache()
{
    char buffer[MAX_OSPATH];
    qhandle_t f;

    if (list_.numItems == numDirs_)
        return;

    if (Q_concat(buffer, sizeof(buffer), browse_, "/" COM_DEMOCACHE_NAME) >= sizeof(buffer))
        return;

    FS_OpenFile(buffer, &f, FS_MODE_WRITE);
    if (!f)
        return;

    for (int i = 0; i < 16; i++)
        FS_FPrintf(f, "%02x", hash_[i]);
    FS_FPrintf(f, "\\");

    for (int i = numDirs_; i < list_.numItems; i++) {
        auto *e = static_cast<demoEntry_t *>(list_.items[i]);
        char *map = UI_GetColumn(e->name, COL_MAP);
        char *pov = UI_GetColumn(e->name, COL_POV);
        FS_FPrintf(f, "%s\\%s\\", map, pov);
    }
    FS_CloseFile(f);
}

void DemoBrowserPage::CalcHash(void **list)
{
    struct mdfour md;
    mdfour_begin(&md);
    while (*list) {
        file_info_t *info = static_cast<file_info_t *>(*list++);
        size_t len = sizeof(*info) + strlen(info->name) - 1;
        mdfour_update(&md, (uint8_t *)info, len);
    }
    mdfour_result(&md, hash_);
}

void DemoBrowserPage::BuildList()
{
    S_StopAllSounds();
    Q_strlcpy(status_, "Building list...", sizeof(status_));
    SCR_UpdateScreen();

    int numDirs = 0;
    int numDemos = 0;
    unsigned flags = ui_listalldemos_->integer ? 0 : FS_TYPE_REAL | FS_PATH_GAME;
    void **dirlist = FS_ListFiles(browse_, NULL, flags | FS_SEARCH_DIRSONLY, &numDirs);
    void **demolist = FS_ListFiles(browse_, DEMO_EXTENSIONS, flags | FS_SEARCH_EXTRAINFO, &numDemos);
    numDemos = min(numDemos, MAX_LISTED_FILES - numDirs);

    list_.items.clear();
    list_.numItems = 0;
    list_.curvalue = 0;
    list_.prestep = 0;

    widest_map_ = 3;
    widest_pov_ = 3;
    total_bytes_ = 0;

    if (strcmp(browse_, "/"))
        BuildDir("..", ENTRY_UP);

    if (dirlist) {
        for (int i = 0; i < numDirs; i++) {
            BuildDir(static_cast<const char *>(dirlist[i]), ENTRY_DN);
        }
        FS_FreeList(dirlist);
    }

    numDirs_ = list_.numItems;

    if (demolist) {
        CalcHash(demolist);
        char *cache = LoadCache();
        if (cache) {
            char *p = cache + 32 + 1;
            for (int i = 0; i < numDemos; i++) {
                BuildName(static_cast<file_info_t *>(demolist[i]), &p);
            }
            FS_FreeFile(cache);
        } else {
            for (int i = 0; i < numDemos; i++) {
                BuildName(static_cast<file_info_t *>(demolist[i]), NULL);
                if ((i & 7) == 0) {
                    UpdateLayout();
                    SCR_UpdateScreen();
                }
            }
        }
        WriteCache();
        FS_FreeList(demolist);
    }

    list_.columns[COL_NAME].name = browse_;

    Change();
    if (list_.sortdir)
        Sort();

    UpdateLayout();

    int demos_count = list_.numItems - numDirs_;
    size_t len = Q_scnprintf(status_, sizeof(status_), "%d demo%s, ",
                             demos_count, demos_count == 1 ? "" : "s");
    Com_FormatSizeLong(status_ + len, sizeof(status_) - len, total_bytes_);

    SCR_UpdateScreen();
}

void DemoBrowserPage::FreeList()
{
    for (void *item : list_.items) {
        Z_Free(item);
    }
    list_.items.clear();
    list_.numItems = 0;
}

Sound DemoBrowserPage::Change()
{
    if (!list_.numItems) {
        Q_strlcpy(status_, "No demos found", sizeof(status_));
        return Sound::Beep;
    }

    auto *e = static_cast<demoEntry_t *>(list_.items[list_.curvalue]);
    if (e->type == ENTRY_DEMO) {
        Q_strlcpy(status_, "Press Enter to play demo", sizeof(status_));
    } else {
        Q_strlcpy(status_, "Press Enter to change directory", sizeof(status_));
    }

    return Sound::Silent;
}

Sound DemoBrowserPage::Sort()
{
    auto sizecmp = [](const void *p1, const void *p2) -> int {
        auto *e1 = *(demoEntry_t **)p1;
        auto *e2 = *(demoEntry_t **)p2;
        if (e1->size > e2->size)
            return g_demo_page->list_.sortdir;
        if (e1->size < e2->size)
            return -g_demo_page->list_.sortdir;
        return 0;
    };
    auto timecmp = [](const void *p1, const void *p2) -> int {
        auto *e1 = *(demoEntry_t **)p1;
        auto *e2 = *(demoEntry_t **)p2;
        if (e1->mtime > e2->mtime)
            return g_demo_page->list_.sortdir;
        if (e1->mtime < e2->mtime)
            return -g_demo_page->list_.sortdir;
        return 0;
    };
    auto namecmp = [](const void *p1, const void *p2) -> int {
        auto *e1 = *(demoEntry_t **)p1;
        auto *e2 = *(demoEntry_t **)p2;
        char *s1 = UI_GetColumn(e1->name, g_demo_page->list_.sortcol);
        char *s2 = UI_GetColumn(e2->name, g_demo_page->list_.sortcol);
        return Q_stricmp(s1, s2) * g_demo_page->list_.sortdir;
    };

    switch (list_.sortcol) {
    case COL_NAME:
    case COL_MAP:
    case COL_POV:
        list_.Sort(numDirs_, namecmp);
        break;
    case COL_DATE:
        list_.Sort(numDirs_, timecmp);
        break;
    case COL_SIZE:
        list_.Sort(numDirs_, sizecmp);
        break;
    }

    return Sound::Silent;
}

void DemoBrowserPage::UpdateLayout()
{
    list_.x = 0;
    list_.y = CONCHAR_HEIGHT;
    list_.height = uis.height - CONCHAR_HEIGHT * 2 - 1;

    int w1 = 17 + widest_map_ + widest_pov_;
    int w2 = uis.width - w1 * CONCHAR_WIDTH - MLIST_PADDING * 4 - MLIST_SCROLLBAR_WIDTH;
    if (w2 > 8 * CONCHAR_WIDTH) {
        list_.columns[COL_NAME].width = w2;
        list_.columns[COL_DATE].width = 12 * CONCHAR_WIDTH + MLIST_PADDING;
        list_.columns[COL_SIZE].width = 5 * CONCHAR_WIDTH + MLIST_PADDING;
        list_.columns[COL_MAP].width = widest_map_ * CONCHAR_WIDTH + MLIST_PADDING;
        list_.columns[COL_POV].width = widest_pov_ * CONCHAR_WIDTH + MLIST_PADDING;
    } else {
        w2 = uis.width - 17 * CONCHAR_WIDTH - MLIST_PADDING * 2 - MLIST_SCROLLBAR_WIDTH;
        list_.columns[COL_NAME].width = w2;
        list_.columns[COL_DATE].width = 12 * CONCHAR_WIDTH + MLIST_PADDING;
        list_.columns[COL_SIZE].width = 5 * CONCHAR_WIDTH + MLIST_PADDING;
        list_.columns[COL_MAP].width = 0;
        list_.columns[COL_POV].width = 0;
    }

    list_.width = 0;
    for (const auto &col : list_.columns)
        list_.width += col.width;
    list_.width += MLIST_SCROLLBAR_WIDTH;
    list_.Init();
}

bool DemoBrowserPage::FileNameOk(const char *s) const
{
    while (*s) {
        if (*s == '\n' || *s == '"' || *s == ';')
            return false;
        s++;
    }
    return true;
}

Sound DemoBrowserPage::LeaveDirectory()
{
    char *s = strrchr(browse_, '/');
    if (!s)
        return Sound::Beep;

    if (s == browse_) {
        Q_strlcpy(browse_, "/", sizeof(browse_));
    } else {
        *s = 0;
    }

    FreeList();
    BuildList();
    list_.Init();

    for (int i = 0; i < numDirs_; i++) {
        auto *e = static_cast<demoEntry_t *>(list_.items[i]);
        if (!strcmp(e->name, s + 1)) {
            list_.SetValue(i);
            break;
        }
    }

    return Sound::Out;
}

Sound DemoBrowserPage::EnterDirectory(demoEntry_t *e)
{
    size_t baselen = strlen(browse_);
    size_t len = strlen(e->name);
    if (baselen + 1 + len >= sizeof(browse_))
        return Sound::Beep;
    if (!FileNameOk(e->name))
        return Sound::Beep;

    if (baselen == 0 || browse_[baselen - 1] != '/')
        browse_[baselen++] = '/';

    memcpy(browse_ + baselen, e->name, len + 1);

    FreeList();
    BuildList();
    list_.Init();
    return Sound::In;
}

Sound DemoBrowserPage::PlayDemo(demoEntry_t *e)
{
    if (strlen(browse_) + 1 + strlen(e->name) >= sizeof(browse_))
        return Sound::Beep;
    if (!FileNameOk(e->name))
        return Sound::Beep;

    Cbuf_AddText(&cmd_buffer, va("demo \"%s/%s\"\n", browse_, e->name));
    return Sound::Silent;
}

Sound DemoBrowserPage::Activate()
{
    if (!list_.numItems)
        return Sound::Beep;

    auto *e = static_cast<demoEntry_t *>(list_.items[list_.curvalue]);
    switch (e->type) {
    case ENTRY_UP:
        return LeaveDirectory();
    case ENTRY_DN:
        return EnterDirectory(e);
    case ENTRY_DEMO:
        return PlayDemo(e);
    }

    return Sound::NotHandled;
}

void DemoBrowserPage::OnOpen()
{
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (tm)
        year_ = tm->tm_year;

    UI_Sys_UpdateGameDir();

    if (strcmp(browse_, "/")
        && ui_listalldemos_->integer == 0
        && os_access(va("%s%s", fs_gamedir, browse_), F_OK)) {
        Q_strlcpy(browse_, "/", sizeof(browse_));
    }

    BuildList();
    UpdateSortFromCvar();
    list_.SetValue(selection_);
}

void DemoBrowserPage::OnClose()
{
    selection_ = list_.curvalue;
    FreeList();
}

void DemoBrowserPage::Draw()
{
    UpdateLayout();
    if (uis.backgroundHandle) {
        R_DrawKeepAspectPic(0, 0, uis.width, uis.height, COLOR_WHITE, uis.backgroundHandle);
    } else {
        R_DrawFill32(0, 0, uis.width, uis.height, uis.color.background);
    }

    UI_DrawString(uis.width / 2, 0, UI_CENTER | UI_ALTCOLOR, COLOR_WHITE, title_);
    list_.Draw();

    if (uis.canvas_width >= 640) {
        UI_DrawString(uis.width, uis.height - CONCHAR_HEIGHT, UI_RIGHT, COLOR_WHITE, status_);
    }
}

Sound DemoBrowserPage::KeyEvent(int key)
{
    if (key == K_BACKSPACE) {
        return LeaveDirectory();
    }
    return list_.KeyEvent(key);
}

void DemoBrowserPage::MouseEvent(int x, int y, bool down)
{
    (void)down;
    list_.HoverAt(x, y);
}

std::unique_ptr<MenuPage> CreateDemoBrowserPage()
{
    return std::make_unique<DemoBrowserPage>();
}

} // namespace ui
