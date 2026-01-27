#include "ui/ui_internal.h"

#include "common/files.h"
#include "common/net/net.h"
#include "system/system.h"

namespace ui {

namespace {

constexpr int kServerListFontSize = 6;
constexpr int kServerListRowPadding = 2;
constexpr float kServerListAlternateShade = 0.8f;

int ServerListTextHeight()
{
    return max(1, UI_FontLineHeightSized(kServerListFontSize));
}

int ServerListSpacing()
{
    return ServerListTextHeight() + kServerListRowPadding;
}

int ServerHintReserveHeight()
{
    int textHeight = CONCHAR_HEIGHT;
    int iconHeight = max(1, textHeight * 2);
    return max(textHeight, iconHeight);
}

bool IsLikelyTextServerList(const char *data, size_t len)
{
    if (!data || len == 0)
        return false;

    size_t check_len = min(len, static_cast<size_t>(256));
    bool saw_digit = false;
    bool saw_dot = false;
    bool saw_colon = false;
    bool saw_newline = false;
    int nonprintable = 0;

    for (size_t i = 0; i < check_len; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == '\n' || c == '\r') {
            saw_newline = true;
            continue;
        }
        if (c < 32 || c > 126) {
            nonprintable++;
            continue;
        }
        if (c >= '0' && c <= '9')
            saw_digit = true;
        if (c == '.')
            saw_dot = true;
        if (c == ':')
            saw_colon = true;
    }

    if (nonprintable > 0)
        return false;

    return saw_digit && (saw_dot || saw_colon) && (saw_newline || len < 64);
}

} // namespace

#define MAX_STATUS_RULES 64
#define MAX_STATUS_SERVERS 1024
#define SLOT_EXTRASIZE offsetof(serverslot_t, name)

#define COL_NAME 0
#define COL_MOD 1
#define COL_MAP 2
#define COL_PLAYERS 3
#define COL_RTT 4
#define COL_MAX 5

#define PING_STAGES 3

enum serverslot_status_t {
    SLOT_IDLE,
    SLOT_PENDING,
    SLOT_ERROR,
    SLOT_VALID
};

struct serverslot_t {
    serverslot_status_t status;
    netadr_t address;
    char *hostname;
    int numRules;
    char *rules[MAX_STATUS_RULES];
    int numPlayers;
    char *players[MAX_STATUS_PLAYERS];
    unsigned timestamp;
    color_t color;
    char name[1];
};

class ServerBrowserPage : public MenuPage {
public:
    ServerBrowserPage();
    ~ServerBrowserPage() override { ClearServers(); Z_Freep(&args_); }
    const char *Name() const override { return "servers"; }
    void OnOpen() override;
    void OnClose() override;
    void Draw() override;
    Sound KeyEvent(int key) override;
    void MouseEvent(int x, int y, bool down) override;
    void Frame(int msec) override;
    void StatusEvent(const serverStatus_t *status) override;
    void ErrorEvent(const netadr_t *from) override;
    void SetArgs(const char *args) override;
    ListWidget &List() { return list_; }
    cvar_t *SortCvar() const { return ui_sortservers_; }

private:
    void ClearServers();
    void UpdateSelection();
    void UpdateStatus();
    serverslot_t *FindSlot(const netadr_t *search, int *index_p);
    void FreeSlot(serverslot_t *slot);
    color_t ColorForStatus(const serverStatus_t *status, unsigned ping);
    void ParseMasterArgs(netadr_t *broadcast);
    void PingServers();
    void FinishPingStage();
    void CalcPingRate();
    Sound Connect();
    Sound PingSelected();
    void AddServer(const netadr_t *address, const char *hostname);
    void ParsePlain(void *data, size_t len, size_t chunk);
    void ParseBinary(void *data, size_t len, size_t chunk);
    void ParseAddressBook();
    Sound SetRconAddress();
    Sound CopyAddress();
    void BuildColumns();

    ListWidget list_{};
    ListWidget info_{};
    ListWidget players_{};
    char *args_ = nullptr;
    unsigned timestamp_ = 0;
    int pingstage_ = 0;
    int pingindex_ = 0;
    int pingtime_ = 0;
    int pingextra_ = 0;
    const char *status_c_ = nullptr;
    char status_r_[32]{};

    cvar_t *ui_sortservers_ = nullptr;
    cvar_t *ui_colorservers_ = nullptr;
    cvar_t *ui_pingrate_ = nullptr;
};

static ServerBrowserPage *g_servers_page;

static int namecmp(const void *p1, const void *p2)
{
    auto *s1 = *(serverslot_t **)p1;
    auto *s2 = *(serverslot_t **)p2;
    char *n1 = UI_GetColumn(s1->name, COL_NAME);
    char *n2 = UI_GetColumn(s2->name, COL_NAME);
    return Q_stricmp(n1, n2) * g_servers_page->List().sortdir;
}

static int mapcmp(const void *p1, const void *p2)
{
    auto *s1 = *(serverslot_t **)p1;
    auto *s2 = *(serverslot_t **)p2;
    char *n1 = UI_GetColumn(s1->name, COL_MAP);
    char *n2 = UI_GetColumn(s2->name, COL_MAP);
    return Q_stricmp(n1, n2) * g_servers_page->List().sortdir;
}

static int modcmp(const void *p1, const void *p2)
{
    auto *s1 = *(serverslot_t **)p1;
    auto *s2 = *(serverslot_t **)p2;
    char *n1 = UI_GetColumn(s1->name, COL_MOD);
    char *n2 = UI_GetColumn(s2->name, COL_MOD);
    return Q_stricmp(n1, n2) * g_servers_page->List().sortdir;
}

static int rttcmp(const void *p1, const void *p2)
{
    auto *s1 = *(serverslot_t **)p1;
    auto *s2 = *(serverslot_t **)p2;
    char *n1 = UI_GetColumn(s1->name, COL_RTT);
    char *n2 = UI_GetColumn(s2->name, COL_RTT);
    return (Q_atoi(n1) - Q_atoi(n2)) * g_servers_page->List().sortdir;
}

static int playerscmp(const void *p1, const void *p2)
{
    auto *s1 = *(serverslot_t **)p1;
    auto *s2 = *(serverslot_t **)p2;
    int p1c = Q_atoi(UI_GetColumn(s1->name, COL_PLAYERS));
    int p2c = Q_atoi(UI_GetColumn(s2->name, COL_PLAYERS));
    return (p1c - p2c) * g_servers_page->List().sortdir;
}

static void ui_sortservers_changed(cvar_t *self)
{
    (void)self;
    if (!g_servers_page)
        return;

    int i = Cvar_ClampInteger(g_servers_page->SortCvar(), -COL_MAX, COL_MAX);
    if (i > 0) {
        g_servers_page->List().sortdir = 1;
        g_servers_page->List().sortcol = i - 1;
    } else if (i < 0) {
        g_servers_page->List().sortdir = -1;
        g_servers_page->List().sortcol = -i - 1;
    } else {
        g_servers_page->List().sortdir = 0;
        g_servers_page->List().sortcol = 0;
    }

    if (g_servers_page->List().sortdir) {
        switch (g_servers_page->List().sortcol) {
        case COL_NAME:
            g_servers_page->List().Sort(0, namecmp);
            break;
        case COL_MOD:
            g_servers_page->List().Sort(0, modcmp);
            break;
        case COL_MAP:
            g_servers_page->List().Sort(0, mapcmp);
            break;
        case COL_PLAYERS:
            g_servers_page->List().Sort(0, playerscmp);
            break;
        case COL_RTT:
            g_servers_page->List().Sort(0, rttcmp);
            break;
        }
    }
}

ServerBrowserPage::ServerBrowserPage()
{
    ui_sortservers_ = Cvar_Get("ui_sortservers", "1", 0);
    ui_sortservers_->changed = ui_sortservers_changed;
    ui_colorservers_ = Cvar_Get("ui_colorservers", "1", 0);
    ui_pingrate_ = Cvar_Get("ui_pingrate", "0", 0);

    list_.mlFlags = MLF_HEADER | MLF_SCROLLBAR | MLF_COLOR;
    list_.extrasize = SLOT_EXTRASIZE;
    list_.columns.resize(COL_MAX);
    list_.columns[COL_NAME].name = "Server";
    list_.columns[COL_NAME].uiFlags = UI_LEFT;
    list_.columns[COL_MOD].name = "Mod";
    list_.columns[COL_MOD].uiFlags = UI_LEFT;
    list_.columns[COL_MAP].name = "Map";
    list_.columns[COL_MAP].uiFlags = UI_LEFT;
    list_.columns[COL_PLAYERS].name = "Players";
    list_.columns[COL_PLAYERS].uiFlags = UI_RIGHT;
    list_.columns[COL_RTT].name = "Ping";
    list_.columns[COL_RTT].uiFlags = UI_RIGHT;

    info_.mlFlags = MLF_HEADER | MLF_SCROLLBAR;
    info_.extrasize = 0;
    info_.columns.resize(2);
    info_.columns[0].name = "Rule";
    info_.columns[0].uiFlags = UI_LEFT;
    info_.columns[1].name = "Value";
    info_.columns[1].uiFlags = UI_LEFT;

    players_.mlFlags = MLF_HEADER | MLF_SCROLLBAR;
    players_.extrasize = 0;
    players_.columns.resize(3);
    players_.columns[0].name = "S";
    players_.columns[0].uiFlags = UI_RIGHT;
    players_.columns[1].name = "P";
    players_.columns[1].uiFlags = UI_RIGHT;
    players_.columns[2].name = "Name";
    players_.columns[2].uiFlags = UI_LEFT;

    list_.onChange = [this](ListWidget *) {
        UpdateSelection();
        return Sound::Silent;
    };
    list_.onActivate = [this](ListWidget *) { return Connect(); };
    info_.onActivate = [this](ListWidget *) { return Connect(); };
    players_.onActivate = [this](ListWidget *) { return Connect(); };

    g_servers_page = this;
}

void ServerBrowserPage::SetArgs(const char *args)
{
    Z_Freep(&args_);
    if (!args)
        return;
    char *copy = UI_CopyString(args);
    args_ = COM_StripQuotes(copy);
}

void ServerBrowserPage::FreeSlot(serverslot_t *slot)
{
    for (int i = 0; i < slot->numRules; i++)
        Z_Free(slot->rules[i]);
    for (int i = 0; i < slot->numPlayers; i++)
        Z_Free(slot->players[i]);
    Z_Free(slot);
}

void ServerBrowserPage::ClearServers()
{
    for (void *item : list_.items) {
        auto *slot = static_cast<serverslot_t *>(item);
        if (slot->hostname)
            Z_Free(slot->hostname);
        FreeSlot(slot);
    }
    list_.items.clear();
    list_.numItems = 0;
    info_.items.clear();
    info_.numItems = 0;
    players_.items.clear();
    players_.numItems = 0;
}

void ServerBrowserPage::UpdateSelection()
{
    serverslot_t *slot = nullptr;
    if (list_.numItems && list_.curvalue >= 0)
        slot = static_cast<serverslot_t *>(list_.items[list_.curvalue]);

    if (!list_.numItems) {
        status_c_ = "No servers found; Press Space to refresh";
    } else if (slot && slot->status == SLOT_VALID) {
        status_c_ = "Press Enter to connect; Space to refresh";
    } else if (pingstage_) {
        status_c_ = "Pinging servers; Press Backspace to abort";
    } else {
        status_c_ = "Select a server; Press Alt+Space to refresh";
    }

    if (slot && slot->status == SLOT_VALID && slot->numRules && uis.canvas_width >= 640) {
        info_.items.assign(slot->rules, slot->rules + slot->numRules);
        info_.numItems = slot->numRules;
        info_.curvalue = -1;
        info_.prestep = 0;
    } else {
        info_.items.clear();
        info_.numItems = 0;
    }

    if (slot && slot->status == SLOT_VALID && slot->numPlayers) {
        players_.items.assign(slot->players, slot->players + slot->numPlayers);
        players_.numItems = slot->numPlayers;
        players_.curvalue = -1;
        players_.prestep = 0;
    } else {
        players_.items.clear();
        players_.numItems = 0;
    }
}

void ServerBrowserPage::UpdateStatus()
{
    int totalplayers = 0;
    int totalservers = 0;

    for (int i = 0; i < list_.numItems; i++) {
        auto *slot = static_cast<serverslot_t *>(list_.items[i]);
        if (slot->status == SLOT_VALID) {
            totalservers++;
            totalplayers += slot->numPlayers;
        }
    }

    Q_snprintf(status_r_, sizeof(status_r_),
               "%d player%s on %d server%s",
               totalplayers, totalplayers == 1 ? "" : "s",
               totalservers, totalservers == 1 ? "" : "s");
}

serverslot_t *ServerBrowserPage::FindSlot(const netadr_t *search, int *index_p)
{
    serverslot_t *found = nullptr;
    int i;

    for (i = 0; i < list_.numItems; i++) {
        auto *slot = static_cast<serverslot_t *>(list_.items[i]);
        if (!NET_IsEqualBaseAdr(search, &slot->address))
            continue;
        if (search->port && search->port != slot->address.port)
            continue;
        found = slot;
        break;
    }

    if (index_p)
        *index_p = i;
    return found;
}

color_t ServerBrowserPage::ColorForStatus(const serverStatus_t *status, unsigned ping)
{
    if (Q_atoi(Info_ValueForKey(status->infostring, "needpass")) >= 1)
        return uis.color.disabled;
    if (Q_atoi(Info_ValueForKey(status->infostring, "anticheat")) >= 2)
        return uis.color.disabled;
    if (Q_stricmp(Info_ValueForKey(status->infostring, "NoFake"), "ENABLED") == 0)
        return uis.color.disabled;
    if (ping < 30)
        return COLOR_GREEN;
    return COLOR_WHITE;
}

void ServerBrowserPage::StatusEvent(const serverStatus_t *status)
{
    UI_Sys_UpdateTimes();
    UI_Sys_UpdateNetFrom();

    if (!args_)
        return;

    int index = 0;
    serverslot_t *slot = FindSlot(&net_from, &index);
    char *hostname = nullptr;
    unsigned timestamp = 0;

    if (!slot) {
        if (list_.numItems >= MAX_STATUS_SERVERS)
            return;
        list_.numItems++;
        hostname = UI_CopyString(NET_AdrToString(&net_from));
        timestamp = timestamp_;
    } else {
        hostname = slot->hostname;
        timestamp = slot->timestamp;
        FreeSlot(slot);
    }

    const char *info = status->infostring;
    const char *host = Info_ValueForKey(info, "hostname");
    if (COM_IsWhite(host))
        host = hostname;
    const char *mod = Info_ValueForKey(info, "game");
    if (COM_IsWhite(mod))
        mod = "baseq2";
    const char *map = Info_ValueForKey(info, "mapname");
    if (COM_IsWhite(map))
        map = "???";
    const char *maxclients = Info_ValueForKey(info, "maxclients");
    if (!COM_IsUint(maxclients))
        maxclients = "?";

    if (timestamp > com_eventTime)
        timestamp = com_eventTime;

    unsigned ping = com_eventTime - timestamp;
    if (ping > 999)
        ping = 999;

    slot = static_cast<serverslot_t *>(UI_FormatColumns(
        SLOT_EXTRASIZE, host, mod, map,
        va("%d/%s", status->numPlayers, maxclients),
        va("%u", ping), NULL));
    slot->status = SLOT_VALID;
    slot->address = net_from;
    slot->hostname = hostname;
    slot->color = ColorForStatus(status, ping);

    if (index >= static_cast<int>(list_.items.size()))
        list_.items.resize(index + 1);
    list_.items[index] = slot;

    slot->numRules = 0;
    char key[MAX_INFO_STRING];
    char value[MAX_INFO_STRING];
    while (slot->numRules < MAX_STATUS_RULES) {
        Info_NextPair(&info, key, value);
        if (!info)
            break;
        if (!key[0])
            Q_strlcpy(key, "<MISSING KEY>", sizeof(key));
        if (!value[0])
            Q_strlcpy(value, "<MISSING VALUE>", sizeof(value));
        slot->rules[slot->numRules++] =
            static_cast<char *>(UI_FormatColumns(0, key, value, NULL));
    }

    slot->numPlayers = status->numPlayers;
    for (int i = 0; i < status->numPlayers; i++) {
        slot->players[i] = static_cast<char *>(UI_FormatColumns(
            0, va("%d", status->players[i].score),
            va("%d", status->players[i].ping),
            status->players[i].name, NULL));
    }

    slot->timestamp = timestamp;

    if (pingstage_)
        ui_sortservers_changed(ui_sortservers_);

    UpdateStatus();
    UpdateSelection();
}

void ServerBrowserPage::ErrorEvent(const netadr_t *from)
{
    UI_Sys_UpdateTimes();
    if (!args_)
        return;

    int index = 0;
    serverslot_t *slot = FindSlot(from, &index);
    if (!slot || slot->status != SLOT_PENDING)
        return;

    netadr_t address = slot->address;
    char *hostname = slot->hostname;
    unsigned timestamp = slot->timestamp;
    FreeSlot(slot);

    if (timestamp > com_eventTime)
        timestamp = com_eventTime;

    unsigned ping = com_eventTime - timestamp;
    if (ping > 999)
        ping = 999;

    slot = static_cast<serverslot_t *>(UI_FormatColumns(
        SLOT_EXTRASIZE, hostname, "???", "???", "down", va("%u", ping), NULL));
    slot->status = SLOT_ERROR;
    slot->address = address;
    slot->hostname = hostname;
    slot->color = COLOR_WHITE;
    slot->numRules = 0;
    slot->numPlayers = 0;
    slot->timestamp = timestamp;

    if (index >= static_cast<int>(list_.items.size()))
        list_.items.resize(index + 1);
    list_.items[index] = slot;
}

Sound ServerBrowserPage::SetRconAddress()
{
    if (!list_.numItems || list_.curvalue < 0)
        return Sound::Beep;

    auto *slot = static_cast<serverslot_t *>(list_.items[list_.curvalue]);
    if (slot->status == SLOT_ERROR)
        return Sound::Beep;

    Cvar_Set("rcon_address", slot->hostname);
    return Sound::Out;
}

Sound ServerBrowserPage::CopyAddress()
{
    if (!list_.numItems || list_.curvalue < 0)
        return Sound::Beep;

    auto *slot = static_cast<serverslot_t *>(list_.items[list_.curvalue]);
    if (slot->status == SLOT_ERROR)
        return Sound::Beep;

    char addr[MAX_QPATH];
    Q_strlcpy(addr, NET_AdrToString(&slot->address), sizeof(addr));
    UI_SetClipboardData(addr);
    return Sound::Out;
}

void ServerBrowserPage::ParseMasterArgs(netadr_t *broadcast)
{
    if (!args_) {
        if (broadcast)
            broadcast->type = NA_UNSPECIFIED;
        return;
    }
    void *data = nullptr;
    int len = 0;
    bool parse_binary = false;
    size_t chunk = 0;
    char *s = nullptr;
    char *p = nullptr;

    Cmd_TokenizeString(args_, false);

    int argc = Cmd_Argc();
    if (!argc) {
        ParseAddressBook();
        if (broadcast) {
            broadcast->type = NA_BROADCAST;
            broadcast->port = BigShort(PORT_SERVER);
        }
        return;
    }

    for (int i = 0; i < argc; i++) {
        s = Cmd_Argv(i);
        if (!*s)
            continue;

        parse_binary = false;
        chunk = 0;
        if (*s == '+' || *s == '-') {
            parse_binary = true;
            chunk = strtoul(s, &p, 10);
            if (s == p) {
                chunk = 6;
                s = p + 1;
            } else {
                if (chunk < 6)
                    goto ignore;
                s = p;
            }
        }

        if (!strncmp(s, "file://", 7)) {
            len = FS_LoadFile(s + 7, &data);
            if (len < 0)
                continue;
            bool parse_as_text = !parse_binary;
            if (parse_binary) {
                size_t data_len = static_cast<size_t>(len);
                if (chunk == 0 || (data_len % chunk) != 0 || IsLikelyTextServerList(static_cast<const char *>(data), data_len))
                    parse_as_text = true;
            }
            if (parse_as_text)
                ParsePlain(data, len, chunk);
            else
                ParseBinary(data, len, chunk);
            FS_FreeFile(data);
            continue;
        }

        if (!strncmp(s, "http://", 7) || !strncmp(s, "https://", 8)) {
#if USE_CURL
            len = HTTP_FetchFile(s, &data);
            if (len < 0)
                continue;
            bool parse_as_text = !parse_binary;
            if (parse_binary) {
                size_t data_len = static_cast<size_t>(len);
                if (chunk == 0 || (data_len % chunk) != 0 || IsLikelyTextServerList(static_cast<const char *>(data), data_len))
                    parse_as_text = true;
            }
            if (parse_as_text)
                ParsePlain(data, len, chunk);
            else
                ParseBinary(data, len, chunk);
            HTTP_FreeFile(data);
#else
            Com_Printf("$cg_auto_9f78db570fe8", s);
#endif
            continue;
        }

        if (!strncmp(s, "favorites://", 12)) {
            ParseAddressBook();
            continue;
        }

        if (!strncmp(s, "broadcast://", 12)) {
            if (broadcast) {
                broadcast->type = NA_BROADCAST;
                broadcast->port = BigShort(PORT_SERVER);
            }
            continue;
        }

        if (!strncmp(s, "quake2://", 9)) {
            AddServer(NULL, s + 9);
            continue;
        }

ignore:
        Com_Printf("$cg_auto_f8b0044bcfd3", s);
    }
}

void ServerBrowserPage::AddServer(const netadr_t *address, const char *hostname)
{
    netadr_t tmp;
    if (list_.numItems >= MAX_STATUS_SERVERS)
        return;

    if (!address) {
        if (!hostname)
            return;
        if (!NET_StringToAdr(hostname, &tmp, PORT_SERVER)) {
            Com_Printf("$cg_auto_35af01e118a2", hostname);
            return;
        }
        address = &tmp;
    }

    if (FindSlot(address, NULL))
        return;

    if (!hostname)
        hostname = NET_AdrToString(address);

    if (BigShort(address->port) < 1024) {
        Com_Printf("$cg_auto_0f3fc5b0f530", hostname);
        return;
    }

    auto *slot = static_cast<serverslot_t *>(UI_FormatColumns(
        SLOT_EXTRASIZE, hostname, "???", "???", "?/?", "???", NULL));
    slot->status = SLOT_IDLE;
    slot->address = *address;
    slot->hostname = UI_CopyString(hostname);
    slot->color = COLOR_WHITE;
    slot->numRules = 0;
    slot->numPlayers = 0;
    slot->timestamp = com_eventTime;

    list_.items.push_back(slot);
    list_.numItems = static_cast<int>(list_.items.size());
}

void ServerBrowserPage::ParsePlain(void *data, size_t len, size_t chunk)
{
    (void)len;
    (void)chunk;

    if (!data)
        return;

    char *list = static_cast<char *>(data);
    while (*list) {
        char *p = strchr(list, '\n');
        if (p) {
            if (p > list && *(p - 1) == '\r')
                *(p - 1) = 0;
            *p = 0;
        }

        if (*list)
            AddServer(NULL, list);

        if (!p)
            break;
        list = p + 1;
    }
}

void ServerBrowserPage::ParseBinary(void *data, size_t len, size_t chunk)
{
    if (!data)
        return;

    netadr_t address{};
    address.type = NA_IP;

    byte *ptr = static_cast<byte *>(data);
    while (len >= chunk) {
        memcpy(address.ip.u8, ptr, 4);
        memcpy(&address.port, ptr + 4, 2);
        ptr += chunk;
        len -= chunk;

        AddServer(&address, NULL);
    }
}

void ServerBrowserPage::ParseAddressBook()
{
    for (int i = 0; i < MAX_STATUS_SERVERS; i++) {
        cvar_t *var = Cvar_FindVar(va("adr%i", i));
        if (!var)
            break;
        if (var->string[0])
            AddServer(NULL, var->string);
    }
}

void ServerBrowserPage::FinishPingStage()
{
    pingstage_ = 0;
    pingindex_ = 0;
    pingextra_ = 0;

    if (list_.curvalue < 0 && list_.numItems)
        list_.curvalue = 0;

    UpdateSelection();
}

void ServerBrowserPage::CalcPingRate()
{
    cvar_t *info_rate = Cvar_FindVar("rate");
    int rate = Cvar_ClampInteger(ui_pingrate_, 0, 100);

    if (!rate) {
        if (!info_rate)
            info_rate = Cvar_Get("rate", "15000", CVAR_USERINFO | CVAR_ARCHIVE);
        rate = Q_clip(info_rate->integer / 450, 1, 100);
    }

    pingtime_ = (1000 * PING_STAGES) / (rate * pingstage_);
}

void ServerBrowserPage::PingServers()
{
    netadr_t broadcast{};
    S_StopAllSounds();

    if (!args_)
        return;

    ClearServers();
    list_.curvalue = -1;

    UpdateStatus();

    status_c_ = "Resolving servers, please wait...";
    SCR_UpdateScreen();

    ParseMasterArgs(&broadcast);
    timestamp_ = Sys_Milliseconds();
    if (broadcast.type)
        CL_SendStatusRequest(&broadcast);

    if (!list_.numItems) {
        FinishPingStage();
        return;
    }

    pingstage_ = PING_STAGES;
    pingindex_ = 0;
    pingextra_ = 0;
    CalcPingRate();
    UpdateStatus();
    UpdateSelection();
}

Sound ServerBrowserPage::Connect()
{
    if (!list_.numItems || list_.curvalue < 0)
        return Sound::Beep;

    auto *slot = static_cast<serverslot_t *>(list_.items[list_.curvalue]);
    if (slot->status == SLOT_ERROR)
        return Sound::Beep;

    Cbuf_AddText(&cmd_buffer, va("connect %s\n", slot->hostname));
    return Sound::Out;
}

Sound ServerBrowserPage::PingSelected()
{
    if (!list_.numItems || list_.curvalue < 0)
        return Sound::Beep;

    auto *slot = static_cast<serverslot_t *>(list_.items[list_.curvalue]);
    if (slot->status == SLOT_ERROR)
        return Sound::Beep;

    CL_SendStatusRequest(&slot->address);
    return Sound::Silent;
}

void ServerBrowserPage::BuildColumns()
{
    int w = uis.width - MLIST_SCROLLBAR_WIDTH;
    int base = (w - (3 * CONCHAR_WIDTH + MLIST_PADDING) - (5 * CONCHAR_WIDTH + MLIST_PADDING)) / 3;
    int hintHeight = ServerHintReserveHeight();
    int rowSpacing = ServerListSpacing();

    list_.x = 0;
    list_.y = CONCHAR_HEIGHT;
    list_.fontSize = kServerListFontSize;
    list_.alternateRows = true;
    list_.alternateShade = kServerListAlternateShade;
    list_.rowSpacing = rowSpacing;
    list_.height = uis.height - CONCHAR_HEIGHT * 2 - 1 - hintHeight;
    list_.columns[COL_NAME].width = base + MLIST_PADDING;
    list_.columns[COL_MOD].width = base + MLIST_PADDING;
    list_.columns[COL_MAP].width = base + MLIST_PADDING;
    list_.columns[COL_PLAYERS].width = 3 * CONCHAR_WIDTH + MLIST_PADDING;
    list_.columns[COL_RTT].width = 5 * CONCHAR_WIDTH + MLIST_PADDING;
    list_.width = w + MLIST_SCROLLBAR_WIDTH;

    if (uis.canvas_width >= 640) {
        info_.x = w + MLIST_SCROLLBAR_WIDTH;
        info_.y = CONCHAR_HEIGHT;
        info_.fontSize = kServerListFontSize;
        info_.alternateRows = true;
        info_.alternateShade = kServerListAlternateShade;
        info_.rowSpacing = rowSpacing;
        info_.height = (uis.height + 1) / 2 - CONCHAR_HEIGHT - 2;
        int side_width = max(0, uis.width - info_.x);
        int list_width = max(0, side_width - MLIST_SCROLLBAR_WIDTH);
        info_.width = list_width + MLIST_SCROLLBAR_WIDTH;
        info_.columns[0].width = list_width / 3;
        info_.columns[1].width = list_width - info_.columns[0].width;

        players_.x = w + MLIST_SCROLLBAR_WIDTH;
        players_.y = (uis.height + 1) / 2 + 1;
        players_.fontSize = kServerListFontSize;
        players_.alternateRows = true;
        players_.alternateShade = kServerListAlternateShade;
        players_.rowSpacing = rowSpacing;
        players_.height = uis.height - hintHeight - players_.y - CONCHAR_HEIGHT;
        players_.width = list_width + MLIST_SCROLLBAR_WIDTH;
        players_.columns[0].width = 3 * CONCHAR_WIDTH + MLIST_PADDING;
        players_.columns[1].width = 3 * CONCHAR_WIDTH + MLIST_PADDING;
        players_.columns[2].width = 15 * CONCHAR_WIDTH + MLIST_PADDING;
    }

    list_.Init();
    info_.Init();
    players_.Init();
}

void ServerBrowserPage::OnOpen()
{
    if (!args_)
        return;

    BuildColumns();
    PingServers();
    ui_sortservers_changed(ui_sortservers_);
}

void ServerBrowserPage::OnClose()
{
    ClearServers();
    Z_Freep(&args_);
}

void ServerBrowserPage::Draw()
{
    BuildColumns();
    if (uis.backgroundHandle) {
        R_DrawKeepAspectPic(0, 0, uis.width, uis.height, COLOR_WHITE, uis.backgroundHandle);
    } else {
        R_DrawFill32(0, 0, uis.width, uis.height, uis.color.background);
    }

    list_.Draw();
    if (uis.canvas_width >= 640) {
        info_.Draw();
        players_.Draw();
    }

    int hintHeight = ServerHintReserveHeight();
    int statusY = uis.height - hintHeight - CONCHAR_HEIGHT;
    int w = (pingstage_ == PING_STAGES && list_.numItems)
        ? pingindex_ * uis.width / list_.numItems
        : uis.width;
    R_DrawFill8(0, statusY, w, CONCHAR_HEIGHT, 4);
    R_DrawFill8(w, statusY, uis.width - w, CONCHAR_HEIGHT, 0);

    if (status_c_)
        UI_DrawString(uis.width / 2, statusY, UI_CENTER, COLOR_WHITE, status_c_);

    if (uis.canvas_width >= 800 && list_.numItems)
        UI_DrawString(uis.width, statusY, UI_RIGHT, COLOR_WHITE, status_r_);

    if (uis.canvas_width >= 800 && list_.numItems && list_.curvalue >= 0) {
        auto *slot = static_cast<serverslot_t *>(list_.items[list_.curvalue]);
        if (slot->status > SLOT_PENDING)
            UI_DrawString(0, statusY, UI_LEFT, COLOR_WHITE, slot->hostname);
    }

    std::vector<UiHint> hints_left{
        UiHint{ K_ESCAPE, "Back", "Esc" },
        UiHint{ K_SPACE, "Refresh", "Space" }
    };
    std::vector<UiHint> hints_right{
        UiHint{ K_ENTER, "Select", "Enter" }
    };
    UI_DrawHintBar(hints_left, hints_right, uis.height);
}

Sound ServerBrowserPage::KeyEvent(int key)
{
    if (key == K_ESCAPE || key == K_MOUSE2) {
        GetMenuSystem().Pop();
        return Sound::Out;
    }
    if (Key_IsDown(key) > 1)
        return Sound::NotHandled;

    switch (key) {
    case 'r':
        if (Key_IsDown(K_CTRL))
            return SetRconAddress();
        break;
    case 'c':
        if (Key_IsDown(K_CTRL))
            return CopyAddress();
        break;
    case K_SPACE:
        if (Key_IsDown(K_ALT) || !list_.numItems) {
            PingServers();
            return Sound::Silent;
        }
        return PingSelected();
    case K_BACKSPACE:
        if (pingstage_) {
            FinishPingStage();
            return Sound::Out;
        }
        return Sound::Silent;
    default:
        break;
    }

    return list_.KeyEvent(key);
}

void ServerBrowserPage::MouseEvent(int x, int y, bool down)
{
    (void)down;
    bool mouse_down = Key_IsDown(K_MOUSE1) != 0;
    if (list_.HandleMouseDrag(x, y, mouse_down))
        return;
    if (uis.canvas_width >= 640) {
        if (info_.HandleMouseDrag(x, y, mouse_down))
            return;
        if (players_.HandleMouseDrag(x, y, mouse_down))
            return;
    }
    list_.HoverAt(x, y);
}

void ServerBrowserPage::Frame(int msec)
{
    UI_Sys_UpdateTimes();

    if (!pingstage_)
        return;

    pingextra_ += msec;
    if (pingextra_ < pingtime_)
        return;

    pingextra_ -= pingtime_;

    while (pingindex_ < list_.numItems) {
        auto *slot = static_cast<serverslot_t *>(list_.items[pingindex_++]);
        if (slot->status > SLOT_PENDING)
            continue;
        slot->status = SLOT_PENDING;
        slot->timestamp = com_eventTime;
        CL_SendStatusRequest(&slot->address);
        break;
    }

    if (pingindex_ == list_.numItems) {
        pingindex_ = 0;
        if (--pingstage_ == 0)
            FinishPingStage();
        else
            CalcPingRate();
    }
}

std::unique_ptr<MenuPage> CreateServerBrowserPage()
{
    return std::make_unique<ServerBrowserPage>();
}

} // namespace ui
