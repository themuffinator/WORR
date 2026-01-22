// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.
// cg_draw.cpp -- HUD/screen drawing (Quake 3-style layout)
#include "cg_local.h"
#include "cg_wheel.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// Shared engine helpers without pulling shared.h into game-side headers.
static inline int Q_clip(int a, int b, int c)
{
    if (a < b)
        return b;
    if (a > c)
        return c;
    return a;
}

static inline float Q_clipf(float a, float b, float c)
{
    if (a < b)
        return b;
    if (a > c)
        return c;
    return a;
}

static inline int Q_rint(float x)
{
    return x < 0.0f ? static_cast<int>(x - 0.5f) : static_cast<int>(x + 0.5f);
}

constexpr int32_t UI_ALTCOLOR = 1 << 5;
constexpr int32_t UI_DRAWCURSOR = 1 << 10;

extern "C" {
size_t UTF8_CountChars(const char *text, size_t bytes);
size_t UTF8_OffsetForChars(const char *text, size_t chars);
int Cvar_ClampInteger(cvar_t *var, int min, int max);
float Cvar_ClampValue(cvar_t *var, float min, float max);
}

#if defined(CHAR_WIDTH)
#undef CHAR_WIDTH
#endif

constexpr int32_t STAT_MINUS      = 10;  // num frame for '-' stats digit
constexpr const char *sb_nums[2][11] =
{
    {   "num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
        "num_6", "num_7", "num_8", "num_9", "num_minus"
    },
    {   "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
        "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
    }
};

constexpr int32_t CHAR_WIDTH    = 16;
constexpr int32_t CONCHAR_WIDTH = 8;
constexpr int32_t CONCHAR_HEIGHT = 8;
constexpr int32_t UI_DROPSHADOW = 1 << 4;

static int32_t font_y_offset;

constexpr rgba_t alt_color { 112, 255, 52, 255 };
static constexpr rgba_t q3_color_table[8] = {
    { 0, 0, 0, 255 },
    { 255, 0, 0, 255 },
    { 0, 255, 0, 255 },
    { 255, 255, 0, 255 },
    { 0, 0, 255, 255 },
    { 0, 255, 255, 255 },
    { 255, 0, 255, 255 },
    { 255, 255, 255, 255 }
};
static constexpr rgba_t q3_rainbow_colors[26] = {
    { 255, 0, 0, 255 },
    { 255, 64, 0, 255 },
    { 255, 128, 0, 255 },
    { 255, 192, 0, 255 },
    { 255, 255, 0, 255 },
    { 192, 255, 0, 255 },
    { 128, 255, 0, 255 },
    { 64, 255, 0, 255 },
    { 0, 255, 0, 255 },
    { 0, 255, 64, 255 },
    { 0, 255, 128, 255 },
    { 0, 255, 192, 255 },
    { 0, 255, 255, 255 },
    { 0, 192, 255, 255 },
    { 0, 128, 255, 255 },
    { 0, 64, 255, 255 },
    { 0, 0, 255, 255 },
    { 64, 0, 255, 255 },
    { 128, 0, 255, 255 },
    { 192, 0, 255, 255 },
    { 255, 0, 255, 255 },
    { 255, 0, 192, 255 },
    { 255, 0, 128, 255 },
    { 255, 0, 64, 255 },
    { 255, 255, 255, 255 },
    { 128, 128, 128, 255 }
};

static cvar_t *scr_usekfont;
static cvar_t *scr_alpha;
static cvar_t *scr_chathud;

static cvar_t *scr_centertime;
static cvar_t *scr_printspeed;
static cvar_t *cl_notifytime;
static cvar_t *scr_maxlines;
static cvar_t *con_notifytime;
static cvar_t *con_notifylines;
static cvar_t *ui_acc_contrast;
static cvar_t* ui_acc_alttypeface;
static cvar_t *cl_obituary_time;
static cvar_t *cl_obituary_fade;

// static temp data used for hud
static struct
{
    struct {
        struct {
            char    text[24];
        } table_cells[6];
    } table_rows[11]; // just enough to store 8 levels + header + total (+ one slack)

    size_t column_widths[6];
    int32_t num_rows = 0;
    int32_t num_columns = 0;
} hud_temp;

#include <vector>

// max number of centerprints in the rotating buffer
constexpr size_t MAX_CENTER_PRINTS = 4;

struct cl_bind_t {
    std::string bind;
    std::string purpose;
};

struct cl_centerprint_t {
    std::vector<cl_bind_t> binds; // binds

    std::vector<std::string> lines;
    bool        instant; // don't type out

    size_t      current_line; // current line we're typing out
    size_t      line_count; // byte count to draw on current line
    bool        finished; // done typing it out
    uint64_t    time_tick, time_off; // time to remove at
};

inline bool CG_ViewingLayout(const player_state_t *ps)
{
    return ps->stats[STAT_LAYOUTS] & (LAYOUTS_LAYOUT | LAYOUTS_INVENTORY);
}

inline bool CG_InIntermission(const player_state_t *ps)
{
    return ps->stats[STAT_LAYOUTS] & LAYOUTS_INTERMISSION;
}

inline bool CG_HudHidden(const player_state_t *ps)
{
    return ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD;
}

layout_flags_t CG_LayoutFlags(const player_state_t *ps)
{
    return (layout_flags_t) ps->stats[STAT_LAYOUTS];
}

#include <optional>
#include <array>

constexpr size_t MAX_NOTIFY = 8;

struct cl_notify_t {
    std::string     message; // utf8 message
    bool            is_active; // filled or not
    bool            is_chat; // green or not
    uint64_t        time; // rotate us when < CL_Time()
};

constexpr size_t MAX_OBITUARY = 4;
constexpr uint64_t OBITUARY_LIFETIME_DEFAULT_MS = 3000;
constexpr uint64_t OBITUARY_FADE_DEFAULT_MS = 200;
constexpr int32_t OBITUARY_LIFETIME_MAX_MS = 60000;
constexpr int32_t OBITUARY_FADE_MAX_MS = 10000;
constexpr char OBITUARY_META_START = '\x1e';
constexpr char OBITUARY_META_SEP = '\x1f';
constexpr const char *OBITUARY_META_TAG = "OBIT";

struct cl_obituary_t {
    std::string killer;
    std::string victim;
    std::string icon;
    std::string label;
    uint64_t    time = 0;
    float       draw_y = 0.0f;
    bool        has_killer = false;
    bool        has_position = false;
};

struct obituary_template_t {
    const char *key;
    const char *icon;
    const char *label;
    int args;
};

// per-splitscreen client hud storage
struct hud_data_t {
    std::array<cl_centerprint_t, MAX_CENTER_PRINTS> centers; // list of centers
    std::optional<size_t> center_index; // current index we're drawing, or unset if none left
    std::array<cl_notify_t, MAX_NOTIFY> notify; // list of notifies
    std::array<cl_obituary_t, MAX_OBITUARY> obituaries; // kill feed entries
};

static std::array<hud_data_t, MAX_SPLIT_PLAYERS> hud_data;

constexpr size_t HUD_BLOB_SEGMENT_COUNT =
    static_cast<size_t>(CONFIG_HUD_BLOB_END - CONFIG_HUD_BLOB + 1);

struct cg_hud_blob_t {
    std::array<std::string, HUD_BLOB_SEGMENT_COUNT> segments;
    std::string blob;
    bool dirty = false;
    uint32_t flags = 0;
};

struct cg_hud_state_t {
    game_style_t game_style = game_style_t::GAME_STYLE_PVE;
    std::string map_name;
    std::string health_bar_name;
    std::string story;
};

struct cg_scoreboard_row_t {
    int client = -1;
    int score = 0;
    int ping = 0;
    int team = -1;
    uint32_t flags = 0;
    int skin = 0;
};

struct cg_scoreboard_spec_t {
    int client = -1;
    int score = 0;
    int ping = 0;
    int wins = 0;
    int losses = 0;
};

struct cg_scoreboard_data_t {
    bool valid = false;
    scoreboard_mode_t mode = SB_MODE_FFA;
    uint32_t flags = 0;
    int score_limit = 0;
    std::array<int, 2> team_scores{ { 0, 0 } };
    std::string gametype;
    std::string host;
    std::string match_time;
    std::string victor;
    int press_frame = 0;
    bool in_progress = false;
    std::vector<cg_scoreboard_row_t> rows;
    std::array<std::vector<cg_scoreboard_row_t>, 2> team_rows;
    std::vector<cg_scoreboard_spec_t> queued;
    std::vector<cg_scoreboard_spec_t> spectators;
    std::string layout_cache;
    bool layout_dirty = true;
};

struct cg_eou_row_t {
    std::string name;
    int kills = 0;
    int total_kills = 0;
    int secrets = 0;
    int total_secrets = 0;
    int time_ms = 0;
};

struct cg_eou_data_t {
    bool valid = false;
    int press_frame = 0;
    std::vector<cg_eou_row_t> rows;
    std::optional<cg_eou_row_t> totals;
    std::string layout_cache;
    bool layout_dirty = true;
};

static cg_hud_blob_t cg_hud_blob;
static cg_hud_state_t cg_hud_state;
static cg_scoreboard_data_t cg_scoreboard;
static cg_eou_data_t cg_eou;

void CG_ClearCenterprint(int32_t isplit)
{
    hud_data[isplit].center_index = {};
}

void CG_ClearNotify(int32_t isplit)
{
    for (auto &msg : hud_data[isplit].notify)
        msg.is_active = false;
}

static void CG_Scoreboard_Reset()
{
    cg_scoreboard.valid = false;
    cg_scoreboard.mode = SB_MODE_FFA;
    cg_scoreboard.flags = 0;
    cg_scoreboard.score_limit = 0;
    cg_scoreboard.team_scores = { { 0, 0 } };
    cg_scoreboard.gametype.clear();
    cg_scoreboard.host.clear();
    cg_scoreboard.match_time.clear();
    cg_scoreboard.victor.clear();
    cg_scoreboard.press_frame = 0;
    cg_scoreboard.in_progress = false;
    cg_scoreboard.rows.clear();
    cg_scoreboard.team_rows[0].clear();
    cg_scoreboard.team_rows[1].clear();
    cg_scoreboard.queued.clear();
    cg_scoreboard.spectators.clear();
    cg_scoreboard.layout_cache.clear();
    cg_scoreboard.layout_dirty = true;
}

static void CG_HudState_Reset()
{
    cg_hud_state.game_style = game_style_t::GAME_STYLE_PVE;
    cg_hud_state.map_name.clear();
    cg_hud_state.health_bar_name.clear();
    cg_hud_state.story.clear();
}

static void CG_EOU_Reset()
{
    cg_eou.valid = false;
    cg_eou.press_frame = 0;
    cg_eou.rows.clear();
    cg_eou.totals.reset();
    cg_eou.layout_cache.clear();
    cg_eou.layout_dirty = true;
}

static void CG_Hud_ParseBlob(cg_hud_blob_t &blob)
{
    blob.flags = 0;
    CG_Scoreboard_Reset();
    CG_EOU_Reset();

    const char *cursor = blob.blob.c_str();
    while (*cursor) {
        const char *line_end = strchr(cursor, '\n');
        size_t len = line_end ? static_cast<size_t>(line_end - cursor) : strlen(cursor);
        std::string line(cursor, len);
        const char *s = line.c_str();
        const char *token = COM_Parse(&s);

        if (token[0]) {
            if (!strcmp(token, "hud_flags") || !strcmp(token, "statusbar_flags")) {
                const char *value = COM_Parse(&s);
                if (value[0])
                    blob.flags = static_cast<uint32_t>(strtoul(value, nullptr, 0));
            } else if (!strcmp(token, "sb_meta")) {
                const char *mode_token = COM_Parse(&s);
                const char *flags_token = COM_Parse(&s);
                const char *limit_token = COM_Parse(&s);
                const char *red_token = COM_Parse(&s);
                const char *blue_token = COM_Parse(&s);
                const char *gametype_token = COM_Parse(&s);

                int mode_value = mode_token[0] ? atoi(mode_token) : 0;
                if (mode_value < SB_MODE_FFA || mode_value > SB_MODE_DUEL)
                    mode_value = SB_MODE_FFA;

                cg_scoreboard.mode = static_cast<scoreboard_mode_t>(mode_value);
                cg_scoreboard.flags = flags_token[0] ? static_cast<uint32_t>(strtoul(flags_token, nullptr, 0)) : 0;
                cg_scoreboard.score_limit = limit_token[0] ? atoi(limit_token) : 0;
                cg_scoreboard.team_scores[0] = red_token[0] ? atoi(red_token) : 0;
                cg_scoreboard.team_scores[1] = blue_token[0] ? atoi(blue_token) : 0;
                cg_scoreboard.gametype = gametype_token[0] ? gametype_token : "";
                cg_scoreboard.rows.clear();
                cg_scoreboard.team_rows[0].clear();
                cg_scoreboard.team_rows[1].clear();
                cg_scoreboard.queued.clear();
                cg_scoreboard.spectators.clear();
                cg_scoreboard.layout_dirty = true;
                cg_scoreboard.valid = true;
            } else if (!strcmp(token, "sb_host")) {
                const char *value = COM_Parse(&s);
                cg_scoreboard.host = value[0] ? value : "";
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "sb_time")) {
                const char *value = COM_Parse(&s);
                cg_scoreboard.match_time = value[0] ? value : "";
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "sb_victor")) {
                const char *value = COM_Parse(&s);
                cg_scoreboard.victor = value[0] ? value : "";
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "sb_press")) {
                const char *value = COM_Parse(&s);
                cg_scoreboard.press_frame = value[0] ? atoi(value) : 0;
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "sb_state")) {
                const char *value = COM_Parse(&s);
                cg_scoreboard.in_progress = value[0] ? (atoi(value) != 0) : false;
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "sb_row")) {
                const char *client_token = COM_Parse(&s);
                const char *score_token = COM_Parse(&s);
                const char *ping_token = COM_Parse(&s);
                const char *team_token = COM_Parse(&s);
                const char *row_flags_token = COM_Parse(&s);
                const char *skin_token = COM_Parse(&s);

                if (!client_token[0])
                    goto next_line;

                const int client = atoi(client_token);
                if (client < 0 || client >= MAX_CLIENTS)
                    goto next_line;

                cg_scoreboard_row_t row;
                row.client = client;
                row.score = score_token[0] ? atoi(score_token) : 0;
                row.ping = ping_token[0] ? atoi(ping_token) : 0;
                row.team = team_token[0] ? atoi(team_token) : -1;
                row.flags = row_flags_token[0] ? static_cast<uint32_t>(strtoul(row_flags_token, nullptr, 0)) : 0;
                row.skin = skin_token[0] ? atoi(skin_token) : 0;

                if (cg_scoreboard.mode == SB_MODE_TEAM && row.team >= 0 && row.team < 2) {
                    cg_scoreboard.team_rows[static_cast<size_t>(row.team)].push_back(row);
                } else {
                    cg_scoreboard.rows.push_back(row);
                }
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "sb_queue")) {
                const char *client_token = COM_Parse(&s);
                const char *wins_token = COM_Parse(&s);
                const char *losses_token = COM_Parse(&s);

                if (!client_token[0])
                    goto next_line;

                const int client = atoi(client_token);
                if (client < 0 || client >= MAX_CLIENTS)
                    goto next_line;

                cg_scoreboard_spec_t spec;
                spec.client = client;
                spec.wins = wins_token[0] ? atoi(wins_token) : 0;
                spec.losses = losses_token[0] ? atoi(losses_token) : 0;
                cg_scoreboard.queued.push_back(spec);
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "sb_spec")) {
                const char *client_token = COM_Parse(&s);
                const char *score_token = COM_Parse(&s);
                const char *ping_token = COM_Parse(&s);

                if (!client_token[0])
                    goto next_line;

                const int client = atoi(client_token);
                if (client < 0 || client >= MAX_CLIENTS)
                    goto next_line;

                cg_scoreboard_spec_t spec;
                spec.client = client;
                spec.score = score_token[0] ? atoi(score_token) : 0;
                spec.ping = ping_token[0] ? atoi(ping_token) : 0;
                cg_scoreboard.spectators.push_back(spec);
                cg_scoreboard.layout_dirty = true;
            } else if (!strcmp(token, "eou_row")) {
                const char *name_token = COM_Parse(&s);
                const char *kills_token = COM_Parse(&s);
                const char *total_kills_token = COM_Parse(&s);
                const char *secrets_token = COM_Parse(&s);
                const char *total_secrets_token = COM_Parse(&s);
                const char *time_token = COM_Parse(&s);

                if (!name_token[0])
                    goto next_line;

                cg_eou_row_t row;
                row.name = name_token;
                row.kills = kills_token[0] ? atoi(kills_token) : 0;
                row.total_kills = total_kills_token[0] ? atoi(total_kills_token) : 0;
                row.secrets = secrets_token[0] ? atoi(secrets_token) : 0;
                row.total_secrets = total_secrets_token[0] ? atoi(total_secrets_token) : 0;
                row.time_ms = time_token[0] ? atoi(time_token) : 0;
                cg_eou.rows.push_back(row);
                cg_eou.valid = true;
                cg_eou.layout_dirty = true;
            } else if (!strcmp(token, "eou_total")) {
                const char *kills_token = COM_Parse(&s);
                const char *total_kills_token = COM_Parse(&s);
                const char *secrets_token = COM_Parse(&s);
                const char *total_secrets_token = COM_Parse(&s);
                const char *time_token = COM_Parse(&s);

                cg_eou_row_t row;
                row.name = " ";
                row.kills = kills_token[0] ? atoi(kills_token) : 0;
                row.total_kills = total_kills_token[0] ? atoi(total_kills_token) : 0;
                row.secrets = secrets_token[0] ? atoi(secrets_token) : 0;
                row.total_secrets = total_secrets_token[0] ? atoi(total_secrets_token) : 0;
                row.time_ms = time_token[0] ? atoi(time_token) : 0;
                cg_eou.totals = row;
                cg_eou.valid = true;
                cg_eou.layout_dirty = true;
            } else if (!strcmp(token, "eou_press")) {
                const char *value = COM_Parse(&s);
                cg_eou.press_frame = value[0] ? atoi(value) : 0;
                cg_eou.valid = true;
                cg_eou.layout_dirty = true;
            }
        }

        next_line:

        if (!line_end)
            break;
        cursor = line_end + 1;
    }
}

static void CG_Hud_RebuildBlob(cg_hud_blob_t &blob)
{
    blob.blob.clear();
    for (const auto &segment : blob.segments)
        blob.blob += segment;
    blob.dirty = false;
    CG_Hud_ParseBlob(blob);
}

static void CG_Hud_EnsureBlobParsed()
{
    if (cg_hud_blob.dirty)
        CG_Hud_RebuildBlob(cg_hud_blob);
}

static void CG_Hud_Reset()
{
    for (auto &segment : cg_hud_blob.segments)
        segment.clear();
    cg_hud_blob.blob.clear();
    cg_hud_blob.flags = 0;
    cg_hud_blob.dirty = false;
    CG_HudState_Reset();
    CG_Scoreboard_Reset();
    CG_EOU_Reset();
}

static uint32_t CG_Hud_GetFlags()
{
    if (cg_hud_blob.dirty)
        CG_Hud_RebuildBlob(cg_hud_blob);
    return cg_hud_blob.flags;
}

static game_style_t CG_GetGameStyle()
{
    return cg_hud_state.game_style;
}

static const char *CG_Hud_GetMapName()
{
    return cg_hud_state.map_name.c_str();
}

static const char *CG_Hud_GetHealthBarName()
{
    return cg_hud_state.health_bar_name.c_str();
}

static const char *CG_Hud_GetStory()
{
    return cg_hud_state.story.c_str();
}

void CG_Hud_ParseConfigString(int32_t i, const char *s)
{
    if (i == CS_NAME) {
        cg_hud_state.map_name = s ? s : "";
        cg_scoreboard.layout_dirty = true;
        return;
    }

    if (i == CS_GAME_STYLE) {
        int style = (s && *s) ? atoi(s) : static_cast<int>(game_style_t::GAME_STYLE_PVE);
        if (style < static_cast<int>(game_style_t::GAME_STYLE_PVE) ||
            style > static_cast<int>(game_style_t::GAME_STYLE_TDM)) {
            style = static_cast<int>(game_style_t::GAME_STYLE_PVE);
        }
        cg_hud_state.game_style = static_cast<game_style_t>(style);
        return;
    }

    if (i == CONFIG_HEALTH_BAR_NAME) {
        cg_hud_state.health_bar_name = s ? s : "";
        return;
    }

    if (i == CONFIG_STORY) {
        cg_hud_state.story = s ? s : "";
        return;
    }

    if (i < CONFIG_HUD_BLOB || i > CONFIG_HUD_BLOB_END)
        return;

    size_t index = static_cast<size_t>(i - CONFIG_HUD_BLOB);
    cg_hud_blob.segments[index] = s ? s : "";
    cg_hud_blob.dirty = true;
}

// if the top one is expired, cycle the ones ahead backwards (since
// the times are always increasing)
static void CG_Notify_CheckExpire(hud_data_t &data)
{
    while (data.notify[0].is_active && data.notify[0].time < cgi.CL_ClientTime())
    {
        data.notify[0].is_active = false;

        for (size_t i = 1; i < MAX_NOTIFY; i++)
            if (data.notify[i].is_active)
                std::swap(data.notify[i], data.notify[i - 1]);
    }
}

static int CG_GetEffectiveNotifyLines(void)
{
    if (con_notifylines && con_notifylines->integer <= 0)
        return 0;
    if (con_notifytime && con_notifytime->value <= 0.0f)
        return 0;
    if (!scr_maxlines || scr_maxlines->integer <= 0)
        return 0;

    int max_lines = min((int)MAX_NOTIFY, scr_maxlines->integer);
    if (con_notifylines && con_notifylines->integer > 0)
        max_lines = min(max_lines, con_notifylines->integer);

    return max(0, max_lines);
}

// add notify to list
static void CG_AddNotify(hud_data_t &data, const char *msg, bool is_chat)
{
    size_t i = 0;

    const int max_lines = CG_GetEffectiveNotifyLines();
    if (max_lines <= 0)
        return;

    const int max = min(MAX_NOTIFY, (size_t)max_lines);

    for (; i < max; i++)
        if (!data.notify[i].is_active)
            break;

    // none left, so expire the topmost one
    if (i == max)
    {
        data.notify[0].time = 0;
        CG_Notify_CheckExpire(data);
        i = max - 1;
    }
    
    data.notify[i].message.assign(msg);
    data.notify[i].is_active = true;
    data.notify[i].is_chat = is_chat;
    data.notify[i].time = cgi.CL_ClientTime() + (cl_notifytime->value * 1000);
}

static void CG_TrimTrailingNewlines(std::string &text)
{
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
}

static const char *CG_SkipObituaryMetadata(const char *msg)
{
    if (!msg || msg[0] != OBITUARY_META_START)
        return msg;
    if (strncmp(msg + 1, OBITUARY_META_TAG, 4) != 0)
        return msg;
    const char *cursor = msg + 1 + 4;
    if (*cursor != OBITUARY_META_SEP)
        return msg;
    cursor++;

    const char *sep = strchr(cursor, OBITUARY_META_SEP);
    if (!sep)
        return msg;
    cursor = sep + 1;
    sep = strchr(cursor, OBITUARY_META_SEP);
    if (!sep)
        return msg;
    cursor = sep + 1;
    sep = strchr(cursor, OBITUARY_META_START);
    if (!sep)
        return msg;
    return sep + 1;
}

static bool CG_MatchObituaryArgs(const std::string &pattern, const std::string &message, int args,
                                 std::string &out_victim, std::string &out_killer)
{
    constexpr const char *victim_token = "__OBIT_VICTIM__";
    constexpr const char *killer_token = "__OBIT_KILLER__";

    if (args == 1) {
        const size_t pos = pattern.find(victim_token);
        if (pos == std::string::npos)
            return false;

        const std::string prefix = pattern.substr(0, pos);
        const std::string suffix = pattern.substr(pos + strlen(victim_token));
        if (message.size() < prefix.size() + suffix.size())
            return false;
        if (message.compare(0, prefix.size(), prefix) != 0)
            return false;
        if (message.compare(message.size() - suffix.size(), suffix.size(), suffix) != 0)
            return false;

        out_victim = message.substr(prefix.size(), message.size() - prefix.size() - suffix.size());
        return !out_victim.empty();
    }

    if (args != 2)
        return false;

    const size_t pos_victim = pattern.find(victim_token);
    const size_t pos_killer = pattern.find(killer_token);
    if (pos_victim == std::string::npos || pos_killer == std::string::npos || pos_victim == pos_killer)
        return false;

    const bool victim_first = pos_victim < pos_killer;
    const size_t first_pos = victim_first ? pos_victim : pos_killer;
    const size_t second_pos = victim_first ? pos_killer : pos_victim;
    const char *first_token = victim_first ? victim_token : killer_token;
    const char *second_token = victim_first ? killer_token : victim_token;

    const std::string prefix = pattern.substr(0, first_pos);
    const size_t first_end = first_pos + strlen(first_token);
    const std::string between = pattern.substr(first_end, second_pos - first_end);
    const std::string suffix = pattern.substr(second_pos + strlen(second_token));

    if (between.empty())
        return false;
    if (message.size() < prefix.size() + suffix.size())
        return false;
    if (message.compare(0, prefix.size(), prefix) != 0)
        return false;
    if (message.compare(message.size() - suffix.size(), suffix.size(), suffix) != 0)
        return false;

    const std::string remaining = message.substr(prefix.size(), message.size() - prefix.size() - suffix.size());
    const size_t split_pos = remaining.find(between);
    if (split_pos == std::string::npos)
        return false;

    const std::string first_value = remaining.substr(0, split_pos);
    const std::string second_value = remaining.substr(split_pos + between.size());
    if (first_value.empty() || second_value.empty())
        return false;

    if (victim_first) {
        out_victim = first_value;
        out_killer = second_value;
    } else {
        out_killer = first_value;
        out_victim = second_value;
    }

    return true;
}

constexpr const char *OBIT_KEY_EXPIRATION = "obit_expiration";
constexpr const char *OBIT_KEY_SELF_PLASMAGUN = "obit_self_plasmagun";
constexpr const char *OBIT_KEY_KILL_PLASMAGUN = "obit_kill_plasmagun";
constexpr const char *OBIT_KEY_KILL_PLASMAGUN_SPLASH = "obit_kill_plasmagun_splash";
constexpr const char *OBIT_KEY_KILL_THUNDERBOLT = "obit_kill_thunderbolt";
constexpr const char *OBIT_KEY_KILL_THUNDERBOLT_DISCHARGE = "obit_kill_thunderbolt_discharge";
constexpr const char *OBIT_KEY_SELF_THUNDERBOLT_DISCHARGE = "obit_self_thunderbolt_discharge";
constexpr const char *OBIT_KEY_SELF_TESLA = "obit_self_tesla";

static const obituary_template_t obituary_templates[] = {
    { OBIT_KEY_EXPIRATION, nullptr, "BLOOD", 1 },
    { OBIT_KEY_SELF_PLASMAGUN, "w_plasmarifle", "PLASMA", 1 },
    { OBIT_KEY_KILL_PLASMAGUN, "w_plasmarifle", "PLASMA", 2 },
    { OBIT_KEY_KILL_PLASMAGUN_SPLASH, "w_plasmarifle", "PLASMA", 2 },
    { OBIT_KEY_KILL_THUNDERBOLT, "w_heatbeam", "BOLT", 2 },
    { OBIT_KEY_KILL_THUNDERBOLT_DISCHARGE, "w_heatbeam", "BOLT", 2 },
    { OBIT_KEY_SELF_THUNDERBOLT_DISCHARGE, "w_heatbeam", "BOLT", 1 },
    { OBIT_KEY_SELF_TESLA, "a_tesla", "TESLA", 1 },
    { "$g_mod_generic_suicide", nullptr, "SUICIDE", 1 },
    { "$g_mod_generic_falling", nullptr, "FALL", 1 },
    { "$g_mod_generic_crush", nullptr, "CRUSH", 1 },
    { "$g_mod_generic_water", nullptr, "WATER", 1 },
    { "$g_mod_generic_slime", nullptr, "SLIME", 1 },
    { "$g_mod_generic_lava", nullptr, "LAVA", 1 },
    { "$g_mod_generic_explosive", nullptr, "BOOM", 1 },
    { "$g_mod_generic_exit", nullptr, "EXIT", 1 },
    { "$g_mod_generic_laser", nullptr, "LASER", 1 },
    { "$g_mod_generic_blaster", "w_blaster", "BLASTER", 1 },
    { "$g_mod_generic_hurt", nullptr, "HURT", 1 },
    { "$g_mod_generic_gekk", nullptr, "GEKK", 1 },
    { "$g_mod_generic_died", nullptr, "DIED", 1 },
    { "$g_mod_self_held_grenade", "a_grenades", "GRENADE", 1 },
    { "$g_mod_self_grenade_splash", "a_grenades", "GRENADE", 1 },
    { "$g_mod_self_rocket_splash", "w_rlauncher", "ROCKET", 1 },
    { "$g_mod_self_bfg_blast", "w_bfg", "BFG", 1 },
    { "$g_mod_self_trap", "a_trap", "TRAP", 1 },
    { "$g_mod_self_dopple_explode", "p_doppleganger", "DOPPLE", 1 },
    { "$g_mod_self_default", nullptr, "SUICIDE", 1 },
    { "$g_mod_kill_blaster", "w_blaster", "BLASTER", 2 },
    { "$g_mod_kill_shotgun", "w_shotgun", "SHOTGUN", 2 },
    { "$g_mod_kill_sshotgun", "w_sshotgun", "SSG", 2 },
    { "$g_mod_kill_machinegun", "w_machinegun", "MG", 2 },
    { "$g_mod_kill_chaingun", "w_chaingun", "CHAINGUN", 2 },
    { "$g_mod_kill_grenade", "a_grenades", "GRENADE", 2 },
    { "$g_mod_kill_grenade_splash", "a_grenades", "GRENADE", 2 },
    { "$g_mod_kill_rocket", "w_rlauncher", "ROCKET", 2 },
    { "$g_mod_kill_rocket_splash", "w_rlauncher", "ROCKET", 2 },
    { "$g_mod_kill_hyperblaster", "w_hyperblaster", "HYPER", 2 },
    { "$g_mod_kill_railgun", "w_railgun", "RAIL", 2 },
    { "$g_mod_kill_bfg_laser", "w_bfg", "BFG", 2 },
    { "$g_mod_kill_bfg_blast", "w_bfg", "BFG", 2 },
    { "$g_mod_kill_bfg_effect", "w_bfg", "BFG", 2 },
    { "$g_mod_kill_handgrenade", "a_grenades", "GRENADE", 2 },
    { "$g_mod_kill_handgrenade_splash", "a_grenades", "GRENADE", 2 },
    { "$g_mod_kill_held_grenade", "a_grenades", "GRENADE", 2 },
    { "$g_mod_kill_telefrag", nullptr, "TELEFRAG", 2 },
    { "$g_mod_kill_ripper", "w_ripper", "RIPPER", 2 },
    { "$g_mod_kill_phalanx", "w_phallanx", "PHALANX", 2 },
    { "$g_mod_kill_trap", "a_trap", "TRAP", 2 },
    { "$g_mod_kill_chainfist", "w_chainfist", "CHAINFIST", 2 },
    { "$g_mod_kill_disintegrator", "w_disintegrator", "DISRUPT", 2 },
    { "$g_mod_kill_etf_rifle", "w_etf_rifle", "ETF", 2 },
    { "$g_mod_kill_heatbeam", "w_heatbeam", "HEATBEAM", 2 },
    { "$g_mod_kill_tesla", "a_tesla", "TESLA", 2 },
    { "$g_mod_kill_prox", "a_prox", "PROX", 2 },
    { "$g_mod_kill_nuke", "p_nuke", "NUKE", 2 },
    { "$g_mod_kill_vengeance_sphere", "p_vengeance", "VENGEANCE", 2 },
    { "$g_mod_kill_defender_sphere", "p_defender", "DEFENDER", 2 },
    { "$g_mod_kill_hunter_sphere", "p_hunter", "HUNTER", 2 },
    { "$g_mod_kill_tracker", nullptr, "TRACKER", 2 },
    { "$g_mod_kill_dopple_explode", "p_doppleganger", "DOPPLE", 2 },
    { "$g_mod_kill_dopple_vengeance", "p_doppleganger", "DOPPLE", 2 },
    { "$g_mod_kill_dopple_hunter", "p_doppleganger", "DOPPLE", 2 },
    { "$g_mod_kill_grapple", "w_grapple", "GRAPPLE", 2 },
    { "$g_mod_kill_generic", nullptr, "KILL", 2 },
};

static const obituary_template_t *CG_FindObituaryTemplateByKey(const char *key, size_t key_len)
{
    if (!key || !*key)
        return nullptr;
    for (const auto &templ : obituary_templates) {
        if (!templ.key)
            continue;
        if (strlen(templ.key) != key_len)
            continue;
        if (strncmp(templ.key, key, key_len) == 0)
            return &templ;
    }
    return nullptr;
}

static bool CG_ParseObituaryMetadata(const char *msg, cl_obituary_t &out)
{
    if (!msg || msg[0] != OBITUARY_META_START)
        return false;
    if (strncmp(msg + 1, OBITUARY_META_TAG, 4) != 0)
        return false;
    const char *cursor = msg + 1 + 4;
    if (*cursor != OBITUARY_META_SEP)
        return false;
    cursor++;

    const char *key_start = cursor;
    const char *key_end = strchr(cursor, OBITUARY_META_SEP);
    if (!key_end)
        return false;
    const char *victim_start = key_end + 1;
    const char *victim_end = strchr(victim_start, OBITUARY_META_SEP);
    if (!victim_end)
        return false;
    const char *killer_start = victim_end + 1;
    const char *killer_end = strchr(killer_start, OBITUARY_META_START);
    if (!killer_end)
        return false;

    std::string key(key_start, key_end - key_start);
    std::string victim(victim_start, victim_end - victim_start);
    std::string killer(killer_start, killer_end - killer_start);
    if (victim.empty())
        return false;

    out = {};
    out.victim = victim;
    out.killer = killer;
    out.has_killer = !killer.empty();
    if (!key.empty()) {
        const obituary_template_t *templ = CG_FindObituaryTemplateByKey(key.c_str(), key.size());
        if (templ) {
            if (templ->icon)
                out.icon = templ->icon;
            if (templ->label)
                out.label = templ->label;
        }
    }
    out.time = cgi.CL_ClientRealTime();
    return true;
}

static bool CG_ParseObituaryMessage(const char *msg, cl_obituary_t &out)
{
    std::string message = msg ? msg : "";
    CG_TrimTrailingNewlines(message);
    if (message.empty())
        return false;

    constexpr const char *victim_token = "__OBIT_VICTIM__";
    constexpr const char *killer_token = "__OBIT_KILLER__";
    const char *args[2] = { victim_token, killer_token };

    for (const auto &templ : obituary_templates) {
        std::string pattern = cgi.Localize(templ.key, args, templ.args);
        CG_TrimTrailingNewlines(pattern);
        if (pattern.empty())
            continue;

        std::string victim;
        std::string killer;
        if (!CG_MatchObituaryArgs(pattern, message, templ.args, victim, killer))
            continue;

        out = {};
        out.victim = victim;
        out.killer = killer;
        out.has_killer = templ.args > 1 && !killer.empty();
        if (templ.icon)
            out.icon = templ.icon;
        if (templ.label)
            out.label = templ.label;
        out.time = cgi.CL_ClientRealTime();
        return true;
    }

    return false;
}

static uint64_t CG_ObituaryCvarMs(cvar_t *var, uint64_t fallback_ms, int max_ms)
{
    if (!var)
        return fallback_ms;
    // Small values are likely seconds; keep legacy ms values for larger inputs.
    constexpr float kObituarySecondsThreshold = 100.0f;
    const float raw = var->value;
    if (raw <= 0.0f)
        return 0;
    if (raw >= kObituarySecondsThreshold)
        return static_cast<uint64_t>(Cvar_ClampInteger(var, 0, max_ms));
    const float seconds = Cvar_ClampValue(var, 0.0f, static_cast<float>(max_ms) / 1000.0f);
    return static_cast<uint64_t>(std::lround(seconds * 1000.0f));
}

static uint64_t CG_Obituary_LifetimeMs()
{
    return CG_ObituaryCvarMs(cl_obituary_time, OBITUARY_LIFETIME_DEFAULT_MS, OBITUARY_LIFETIME_MAX_MS);
}

static uint64_t CG_Obituary_FadeMs()
{
    return CG_ObituaryCvarMs(cl_obituary_fade, OBITUARY_FADE_DEFAULT_MS, OBITUARY_FADE_MAX_MS);
}

static void CG_AdvanceObituaryValue(float &value, float target, float speed)
{
    float frame_time = cgi.CL_FrameTime ? cgi.CL_FrameTime() : 0.0f;
    if (value < target) {
        value += speed * frame_time;
        if (value > target)
            value = target;
    } else if (value > target) {
        value -= speed * frame_time;
        if (value < target)
            value = target;
    }
}

static void CG_Obituary_Compact(hud_data_t &data, uint64_t lifetime)
{
    const uint64_t now = cgi.CL_ClientRealTime();
    size_t write_index = 0;

    for (size_t i = 0; i < MAX_OBITUARY; i++) {
        auto &entry = data.obituaries[i];
        if (entry.victim.empty())
            continue;
        if (now >= entry.time + lifetime)
            continue;

        if (write_index != i)
            data.obituaries[write_index] = entry;
        write_index++;
    }

    for (size_t i = write_index; i < MAX_OBITUARY; i++)
        data.obituaries[i] = {};
}

static void CG_AddObituary(hud_data_t &data, const cl_obituary_t &entry)
{
    const uint64_t lifetime = CG_Obituary_LifetimeMs();
    if (!lifetime)
        return;
    CG_Obituary_Compact(data, lifetime);

    size_t count = 0;
    while (count < MAX_OBITUARY && !data.obituaries[count].victim.empty())
        count++;

    if (count == MAX_OBITUARY) {
        for (size_t i = 1; i < MAX_OBITUARY; i++)
            data.obituaries[i - 1] = data.obituaries[i];
        count = MAX_OBITUARY - 1;
    }

    data.obituaries[count] = entry;
}

// draw notifies
static void CG_DrawNotify(int32_t isplit, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale)
{
    auto &data = hud_data[isplit];
    const int max_lines = CG_GetEffectiveNotifyLines();

    if (max_lines <= 0 || !cl_notifytime || cl_notifytime->value <= 0.0f) {
        CG_ClearNotify(isplit);
        return;
    }

    CG_Notify_CheckExpire(data);

    int y;
    
    y = (hud_vrect.y * scale) + hud_safe.y;

    cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);

    if (ui_acc_contrast->integer)
    {
        for (auto& msg : data.notify)
        {
            if (!msg.is_active || !msg.message.length())
                break;

            vec2_t sz = cgi.SCR_MeasureFontString(msg.message.c_str(), scale);
            sz.x += 10; // extra padding for black bars
            cgi.SCR_DrawColorPic((hud_vrect.x * scale) + hud_safe.x - 5, y, sz.x, 15 * scale, "_white", rgba_black);
            y += 10 * scale;
        }
    }

    y = (hud_vrect.y * scale) + hud_safe.y;
    for (auto &msg : data.notify)
    {
        if (!msg.is_active)
            break;

        cgi.SCR_DrawFontString(msg.message.c_str(), (hud_vrect.x * scale) + hud_safe.x, y, scale, msg.is_chat ? alt_color : rgba_white, true, text_align_t::LEFT);
        y += 10 * scale;
    }

    cgi.SCR_SetAltTypeface(false);

    // draw text input (only the main player can really chat anyways...)
    if (isplit == 0)
    {
        const char *input_msg;
        bool input_team;

        if (cgi.CL_GetTextInput(&input_msg, &input_team))
            cgi.SCR_DrawFontString(G_Fmt("{}: {}", input_team ? "say_team" : "say", input_msg).data(), (hud_vrect.x * scale) + hud_safe.x, y, scale, rgba_white, true, text_align_t::LEFT);
    }
}

static void CG_DrawObituaries(int32_t isplit, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale)
{
    auto &data = hud_data[isplit];
    const int max_lines = CG_GetEffectiveNotifyLines();
    const uint64_t lifetime = CG_Obituary_LifetimeMs();
    if (!lifetime) {
        data.obituaries = {};
        return;
    }
    const uint64_t fade_time = min(lifetime, CG_Obituary_FadeMs());
    CG_Obituary_Compact(data, lifetime);

    const int line_height = max(1, static_cast<int>(std::lround(cgi.SCR_FontLineHeight(scale))));
    const int icon_size = max(1, line_height);
    const int padding = max(2, 4 * scale);
    const int spacing = max(1, 2 * scale);

    const int x = (hud_vrect.x * scale) + hud_safe.x;
    float y = (hud_vrect.y * scale) + hud_safe.y;
    if (max_lines > 0)
        y += max_lines * (10 * scale);

    const uint64_t now = cgi.CL_ClientRealTime();
    const float slide_speed = max(40.0f, (line_height + spacing) * 12.0f);

    for (auto &entry : data.obituaries)
    {
        if (entry.victim.empty())
            break;

        const uint64_t age = now - entry.time;
        if (age >= lifetime)
            continue;

        float fade = 1.0f;
        if (fade_time > 0 && age >= (lifetime - fade_time))
            fade = (float)(lifetime - age) / (float)fade_time;

        rgba_t color = rgba_white;
        color.a = static_cast<uint8_t>(std::lround(255.0f * fade));

        const float target_y = y;
        if (!entry.has_position) {
            entry.draw_y = target_y + (float)(line_height + spacing) * 0.5f;
            entry.has_position = true;
        }
        CG_AdvanceObituaryValue(entry.draw_y, target_y, slide_speed);

        const int draw_y = static_cast<int>(std::lround(entry.draw_y));
        int draw_x = x;

        if (entry.has_killer)
        {
            cgi.SCR_DrawFontString(entry.killer.c_str(), draw_x, draw_y, scale, color, true, text_align_t::LEFT);
            draw_x += cgi.SCR_MeasureFontString(entry.killer.c_str(), scale).x + padding;
        }

        bool drew_icon = false;
        if (!entry.icon.empty())
        {
            int icon_w = 0;
            int icon_h = 0;
            cgi.Draw_GetPicSize(&icon_w, &icon_h, entry.icon.c_str());
            if (icon_w > 0 && icon_h > 0)
            {
                const float icon_scale = (float)icon_size / (float)icon_h;
                const int draw_w = max(1, static_cast<int>(std::lround(icon_w * icon_scale)));
                const int draw_h = max(1, static_cast<int>(std::lround(icon_h * icon_scale)));
                const int icon_y = draw_y + (line_height - draw_h) / 2;

                cgi.SCR_DrawColorPic(draw_x, icon_y, draw_w, draw_h, entry.icon.c_str(), color);
                draw_x += draw_w + padding;
                drew_icon = true;
            }
        }

        if (!drew_icon && !entry.label.empty())
        {
            cgi.SCR_DrawFontString(entry.label.c_str(), draw_x, draw_y, scale, color, true, text_align_t::LEFT);
            draw_x += cgi.SCR_MeasureFontString(entry.label.c_str(), scale).x + padding;
        }

        cgi.SCR_DrawFontString(entry.victim.c_str(), draw_x, draw_y, scale, color, true, text_align_t::LEFT);
        y += (float)(line_height + spacing);
    }
}

/*
===============================================================================

CHAT HUD

===============================================================================
*/

constexpr size_t CHAT_MAX_NOTIFY_TEXT = 150;
constexpr size_t CHAT_MAX_NOTIFY_LINES = 32;
constexpr uint64_t CHAT_NOTIFY_LIFETIME_MS = 4000;
constexpr uint64_t CHAT_NOTIFY_FADE_MS = 200;
constexpr int32_t CHAT_NOTIFY_VISIBLE_LINES = 4;
constexpr int32_t CHAT_NOTIFY_VISIBLE_LINES_ACTIVE = 8;
constexpr float CHAT_NOTIFY_SCROLL_SPEED = 12.0f;
constexpr int32_t CHAT_NOTIFY_STATUSBAR_ICON_HEIGHT = (CONCHAR_HEIGHT * 3);
constexpr int32_t CHAT_NOTIFY_STATUSBAR_OFFSET = (CHAT_NOTIFY_STATUSBAR_ICON_HEIGHT * 2);

constexpr int32_t CHAT_KEYDEST_CONSOLE = 1 << 0;
constexpr int32_t CHAT_KEYDEST_MESSAGE = 1 << 1;
constexpr int32_t CHAT_MOUSE1 = 200;

struct cg_chat_line_t {
    char text[CHAT_MAX_NOTIFY_TEXT];
    uint64_t time;
    bool is_chat;
};

struct cg_chat_draw_t {
    cg_chat_line_t *line;
    float alpha;
};

struct cg_chat_layout_t {
    bool valid = false;
    int x = 0;
    int max_chars = 0;
    int width = 0;
    int viewport_top = 0;
    int viewport_bottom = 0;
    int max_visible = 0;
    int total_lines = 0;
    int input_x = 0;
    int input_y = 0;
    int input_w = 0;
    int input_h = 0;
    int prompt_skip = 0;
    int scrollbar_x = 0;
    int scrollbar_w = 0;
    int scroll_track_top = 0;
    int scroll_track_height = 0;
    int thumb_top = 0;
    int thumb_height = 0;
};

struct cg_chat_hud_t {
    std::array<cg_chat_line_t, CHAT_MAX_NOTIFY_LINES> lines{};
    uint32_t head = 0;
    float scroll = 0.0f;
    float scroll_target = 0.0f;
    float scroll_max = 0.0f;
    bool drag = false;
    int drag_offset = 0;
    int mouse_x = 0;
    int mouse_y = 0;
    bool mouse_valid = false;
    cg_chat_layout_t layout{};
};

static std::array<cg_chat_hud_t, MAX_SPLIT_PLAYERS> chat_hud;

static float CG_ChatHud_FadeAlpha(uint64_t start_time, uint64_t vis_time, uint64_t fade_time)
{
    uint64_t now = cgi.CL_ClientRealTime();
    if (now <= start_time)
        return 1.0f;

    uint64_t delta = now - start_time;
    if (delta >= vis_time)
        return 0.0f;

    if (fade_time > vis_time)
        fade_time = vis_time;

    uint64_t time_left = vis_time - delta;
    if (time_left >= fade_time)
        return 1.0f;

    return (float)time_left / (float)fade_time;
}

static void CG_ChatHud_GetSafeRect(const vrect_t &hud_vrect, const vrect_t &hud_safe,
                                  int *out_x, int *out_y, int *out_w, int *out_h)
{
    int inset_x = max(0, hud_safe.x);
    int inset_y = max(0, hud_safe.y);
    int safe_w = max(0, hud_vrect.width - (inset_x * 2));
    int safe_h = max(0, hud_vrect.height - (inset_y * 2));

    if (out_x)
        *out_x = hud_vrect.x + inset_x;
    if (out_y)
        *out_y = hud_vrect.y + inset_y;
    if (out_w)
        *out_w = safe_w;
    if (out_h)
        *out_h = safe_h;
}

static int CG_ChatHud_ClampPromptSkip(int prompt_skip, int max_chars)
{
    int max_prompt = max(0, max_chars - 1);
    if (prompt_skip > max_prompt)
        return max_prompt;
    if (prompt_skip < 0)
        return 0;
    return prompt_skip;
}

static int CG_ChatHud_BuildList(cg_chat_hud_t &hud, cg_chat_draw_t *list, int max_lines, bool message_active)
{
    int stored = hud.head > CHAT_MAX_NOTIFY_LINES ? CHAT_MAX_NOTIFY_LINES : (int)hud.head;
    unsigned start = hud.head - stored;
    int count = 0;

    for (int i = 0; i < stored; i++) {
        cg_chat_line_t *line = &hud.lines[(start + i) % CHAT_MAX_NOTIFY_LINES];
        if (!line->time)
            continue;

        float alpha = 1.0f;
        if (!message_active) {
            alpha = CG_ChatHud_FadeAlpha(line->time, CHAT_NOTIFY_LIFETIME_MS, CHAT_NOTIFY_FADE_MS);
            if (!alpha)
                continue;
        }

        list[count++] = cg_chat_draw_t{ line, alpha };
        if (count == max_lines)
            break;
    }

    return count;
}

static void CG_ChatHud_BuildLayout(cg_chat_layout_t *layout, const vrect_t &hud_vrect, const vrect_t &hud_safe,
                                   int total_lines, int max_visible, bool message_active)
{
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;
    CG_ChatHud_GetSafeRect(hud_vrect, hud_safe, &safe_x, &safe_y, &safe_w, &safe_h);

    int gap = CONCHAR_WIDTH / 2;
    int scrollbar_w = max(2, CONCHAR_WIDTH / 2);
    int available_width = safe_w - scrollbar_w - gap;
    int max_chars = available_width / CONCHAR_WIDTH;

    if (max_chars > 64)
        max_chars = 64;
    if (max_chars < 10)
        max_chars = max(1, max_chars);

    int width = max_chars * CONCHAR_WIDTH;
    int bottom = safe_y + safe_h - CONCHAR_HEIGHT - CHAT_NOTIFY_STATUSBAR_OFFSET;
    if (bottom < safe_y)
        bottom = safe_y;
    int viewport_bottom = message_active ? (bottom - CONCHAR_HEIGHT) : bottom;
    int viewport_top = viewport_bottom - (max_visible - 1) * CONCHAR_HEIGHT;

    layout->valid = true;
    layout->x = safe_x;
    layout->max_chars = max_chars;
    layout->width = width;
    layout->viewport_top = viewport_top;
    layout->viewport_bottom = viewport_bottom;
    layout->max_visible = max_visible;
    layout->total_lines = total_lines;
    layout->input_x = safe_x;
    layout->input_y = bottom;
    layout->input_w = width;
    layout->input_h = CONCHAR_HEIGHT;
    layout->scrollbar_x = safe_x + width + gap;
    layout->scrollbar_w = scrollbar_w;
    layout->scroll_track_top = viewport_top;
    layout->scroll_track_height = max_visible * CONCHAR_HEIGHT;
    layout->thumb_top = viewport_top;
    layout->thumb_height = layout->scroll_track_height;
    layout->prompt_skip = 0;
    if (message_active && cgi.CL_GetChatPrompt) {
        cgi.CL_GetChatPrompt(&layout->prompt_skip);
        layout->prompt_skip = CG_ChatHud_ClampPromptSkip(layout->prompt_skip, layout->max_chars);
    }
}

static void CG_ChatHud_SetScrollFromThumb(cg_chat_hud_t &hud, const cg_chat_layout_t *layout, int thumb_top)
{
    int travel = layout->scroll_track_height - layout->thumb_height;
    if (travel <= 0 || hud.scroll_max <= 0.0f) {
        hud.scroll_target = 0.0f;
        return;
    }

    int clamped = Q_clip(thumb_top, layout->scroll_track_top,
                         layout->scroll_track_top + travel);
    float frac = 1.0f - (float)(clamped - layout->scroll_track_top) / (float)travel;
    hud.scroll_target = frac * hud.scroll_max;
}

static void CG_ChatHud_UpdateDrag(cg_chat_hud_t &hud, const cg_chat_layout_t *layout, bool message_active)
{
    if (!hud.drag)
        return;

    if (!message_active || !cgi.Key_IsDown || !cgi.Key_IsDown(CHAT_MOUSE1)) {
        hud.drag = false;
        return;
    }

    if (!hud.mouse_valid || layout->total_lines <= layout->max_visible) {
        hud.drag = false;
        return;
    }

    int thumb_top = hud.mouse_y - hud.drag_offset;
    CG_ChatHud_SetScrollFromThumb(hud, layout, thumb_top);
}

static void CG_ChatHud_SetCursorFromMouse(cg_chat_hud_t &hud, const cg_chat_layout_t *layout)
{
    if (!cgi.CL_GetChatInput || !cgi.CL_SetChatCursor)
        return;

    cg_chat_input_t input{};
    if (!cgi.CL_GetChatInput(&input))
        return;
    if (!input.text || !input.max_chars || !input.visible_chars)
        return;
    if (layout->prompt_skip >= layout->max_chars)
        return;

    int prompt_skip = 0;
    const char *prompt = cgi.CL_GetChatPrompt ? cgi.CL_GetChatPrompt(&prompt_skip) : "";
    prompt_skip = CG_ChatHud_ClampPromptSkip(prompt_skip, layout->max_chars);
    int prompt_width = cgi.SCR_MeasureString ? cgi.SCR_MeasureString(prompt, prompt_skip) : 0;
    int text_x = layout->input_x + prompt_width;
    int text_w = layout->width - prompt_width;

    if (hud.mouse_x < text_x || hud.mouse_x >= text_x + text_w)
        return;

    size_t cursor_chars = UTF8_CountChars(input.text, input.cursor_pos);
    size_t offset_chars = 0;
    if (cursor_chars >= input.visible_chars) {
        offset_chars = cursor_chars - (input.visible_chars - 1);
    }

    size_t offset = UTF8_OffsetForChars(input.text, offset_chars);
    const char *text = input.text + offset;
    size_t available_chars = UTF8_CountChars(text, strlen(text));
    size_t max_chars = min(input.visible_chars, available_chars);
    int click_x = hud.mouse_x - text_x;
    if (click_x < 0)
        click_x = 0;

    size_t click_bytes = 0;
    for (size_t i = 0; i < max_chars; ++i) {
        size_t len_bytes = UTF8_OffsetForChars(text, i + 1);
        int width = cgi.SCR_MeasureString ? cgi.SCR_MeasureString(text, len_bytes) : 0;
        if (width > click_x)
            break;
        click_bytes = len_bytes;
    }

    size_t new_pos = offset + click_bytes;
    if (new_pos > strlen(input.text))
        new_pos = strlen(input.text);
    if (new_pos >= input.max_chars)
        new_pos = input.max_chars - 1;

    cgi.CL_SetChatCursor(new_pos);
}

static void CG_ChatHud_DrawInputField(const cg_chat_input_t &input, int x, int y, int flags,
                                      size_t max_chars, const rgba_t &color)
{
    if (!input.text || !input.max_chars || !input.visible_chars)
        return;
    if (!cgi.SCR_DrawString || !cgi.SCR_MeasureString || !cgi.SCR_DrawCharStretch)
        return;

    size_t cursor_chars = UTF8_CountChars(input.text, input.cursor_pos);
    size_t offset_chars = 0;
    if (cursor_chars >= input.visible_chars) {
        offset_chars = cursor_chars - (input.visible_chars - 1);
    }

    size_t draw_chars = input.visible_chars;
    if (draw_chars > max_chars)
        draw_chars = max_chars;

    size_t cursor_chars_visible = (cursor_chars > offset_chars) ? (cursor_chars - offset_chars) : 0;
    if (cursor_chars_visible > draw_chars)
        cursor_chars_visible = draw_chars;

    size_t offset = UTF8_OffsetForChars(input.text, offset_chars);
    const char *text = input.text + offset;
    size_t draw_len = UTF8_OffsetForChars(text, draw_chars);
    size_t cursor_bytes = UTF8_OffsetForChars(text, cursor_chars_visible);

    cgi.SCR_DrawString(x, y, 1, flags, draw_len, text, color);

    if ((flags & UI_DRAWCURSOR) && (cgi.CL_ClientRealTime() & (1U << 8))) {
        int cursor_x = x + cgi.SCR_MeasureString(text, cursor_bytes);
        int cursor_ch = input.overstrike ? 11 : '_';
        cgi.SCR_DrawCharStretch(cursor_x, y, CONCHAR_WIDTH, CONCHAR_HEIGHT, flags, cursor_ch, color);
    }
}

static void CG_ChatHud_AdvanceValue(float *val, float target, float speed)
{
    if (!val)
        return;
    float frame_time = cgi.CL_FrameTime ? cgi.CL_FrameTime() : 0.0f;

    if (*val < target) {
        *val += speed * frame_time;
        if (*val > target)
            *val = target;
    } else if (*val > target) {
        *val -= speed * frame_time;
        if (*val < target)
            *val = target;
    }
}

void CG_ChatHud_Clear(int32_t isplit)
{
    if (isplit < 0 || isplit >= (int32_t)chat_hud.size())
        return;

    chat_hud[isplit] = {};
}

void CG_ChatHud_AddLine(int32_t isplit, const char *text, bool is_chat)
{
    if (isplit < 0 || isplit >= (int32_t)chat_hud.size())
        return;
    if (!text || !*text)
        return;

    auto &hud = chat_hud[isplit];
    cg_chat_line_t *line = &hud.lines[hud.head++ % CHAT_MAX_NOTIFY_LINES];
    Q_strlcpy(line->text, text, sizeof(line->text));
    line->time = cgi.CL_ClientRealTime();
    line->is_chat = is_chat;

    char *newline = strrchr(line->text, '\n');
    if (newline)
        *newline = '\0';

    const int key_dest = cgi.CL_GetKeyDest ? cgi.CL_GetKeyDest() : 0;
    if ((key_dest & CHAT_KEYDEST_MESSAGE) && hud.scroll_target > 0.0f) {
        hud.scroll_target += 1.0f;
    }
}

void CG_ChatHud_ScrollLines(float delta)
{
    if (!cgi.CL_GetKeyDest)
        return;

    const int key_dest = cgi.CL_GetKeyDest();
    if (!(key_dest & CHAT_KEYDEST_MESSAGE))
        return;

    auto &hud = chat_hud[0];
    hud.scroll_target = Q_clipf(hud.scroll_target + delta, 0.0f, hud.scroll_max);
}

void CG_ChatHud_MouseEvent(int x, int y)
{
    if (!cgi.SCR_GetScreenMetrics)
        return;

    cg_screen_metrics_t metrics{};
    cgi.SCR_GetScreenMetrics(&metrics);
    float scale = metrics.virtual_scale > 0.0f ? (metrics.hud_scale / metrics.virtual_scale) : 1.0f;

    int hud_width = max(1, metrics.hud_width);
    int hud_height = max(1, metrics.hud_height);

    auto &hud = chat_hud[0];
    hud.mouse_x = Q_clip(Q_rint(x * scale), 0, max(0, hud_width - 1));
    hud.mouse_y = Q_clip(Q_rint(y * scale), 0, max(0, hud_height - 1));
    hud.mouse_valid = true;
}

void CG_ChatHud_MouseDown(int button)
{
    if (button != CHAT_MOUSE1)
        return;
    if (!cgi.CL_GetKeyDest)
        return;

    const int key_dest = cgi.CL_GetKeyDest();
    if (!(key_dest & CHAT_KEYDEST_MESSAGE))
        return;

    auto &hud = chat_hud[0];
    if (!hud.layout.valid || !hud.mouse_valid)
        return;

    if (cgi.CL_GetChatPrompt) {
        cgi.CL_GetChatPrompt(&hud.layout.prompt_skip);
        hud.layout.prompt_skip = CG_ChatHud_ClampPromptSkip(hud.layout.prompt_skip, hud.layout.max_chars);
    }

    if (hud.layout.total_lines > hud.layout.max_visible) {
        int x = hud.mouse_x;
        int y = hud.mouse_y;
        int track_left = hud.layout.scrollbar_x;
        int track_right = hud.layout.scrollbar_x + hud.layout.scrollbar_w;
        int track_top = hud.layout.scroll_track_top;
        int track_bottom = track_top + hud.layout.scroll_track_height;

        if (x >= track_left && x <= track_right && y >= track_top && y <= track_bottom) {
            int thumb_top = hud.layout.thumb_top;
            int thumb_bottom = thumb_top + hud.layout.thumb_height;

            if (y >= thumb_top && y <= thumb_bottom) {
                hud.drag = true;
                hud.drag_offset = y - thumb_top;
            } else {
                int target_top = y - (hud.layout.thumb_height / 2);
                CG_ChatHud_SetScrollFromThumb(hud, &hud.layout, target_top);
                hud.drag = true;
                hud.drag_offset = hud.layout.thumb_height / 2;
            }
            return;
        }
    }

    CG_ChatHud_SetCursorFromMouse(hud, &hud.layout);
}

void CG_DrawChatHUD(int32_t isplit, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale)
{
    if (isplit < 0 || isplit >= (int32_t)chat_hud.size())
        return;

    if (!scr_chathud || scr_chathud->integer == 0) {
        const int key_dest = cgi.CL_GetKeyDest ? cgi.CL_GetKeyDest() : 0;
        if (!(key_dest & CHAT_KEYDEST_MESSAGE))
            return;
    }

    const int key_dest = cgi.CL_GetKeyDest ? cgi.CL_GetKeyDest() : 0;
    if (key_dest & CHAT_KEYDEST_CONSOLE)
        return;

    bool message_active = (key_dest & CHAT_KEYDEST_MESSAGE) != 0;
    int max_visible = message_active ? CHAT_NOTIFY_VISIBLE_LINES_ACTIVE : CHAT_NOTIFY_VISIBLE_LINES;
    auto &hud = chat_hud[isplit];

    cg_chat_draw_t draw_lines[CHAT_MAX_NOTIFY_LINES];
    int total = CG_ChatHud_BuildList(hud, draw_lines, CHAT_MAX_NOTIFY_LINES, message_active);

    if (!message_active) {
        hud.scroll = 0.0f;
        hud.scroll_target = 0.0f;
        hud.scroll_max = 0.0f;
        hud.drag = false;
    }

    CG_ChatHud_BuildLayout(&hud.layout, hud_vrect, hud_safe, total, max_visible, message_active);

    hud.scroll_max = max(0.0f, (float)(total - max_visible));
    if (hud.scroll_max <= 0.0f) {
        hud.scroll = 0.0f;
        hud.scroll_target = 0.0f;
    } else {
        hud.scroll = Q_clipf(hud.scroll, 0.0f, hud.scroll_max);
        hud.scroll_target = Q_clipf(hud.scroll_target, 0.0f, hud.scroll_max);
    }

    if (message_active) {
        CG_ChatHud_UpdateDrag(hud, &hud.layout, message_active);
        CG_ChatHud_AdvanceValue(&hud.scroll, hud.scroll_target, CHAT_NOTIFY_SCROLL_SPEED);
    }

    rgba_t base_color = rgba_white;
    if (scr_alpha)
        base_color.a = (uint8_t)Q_clip(Q_rint(255.0f * Cvar_ClampValue(scr_alpha, 0, 1)), 0, 255);

    if (total > 0) {
        float scroll_pixels = hud.scroll * CONCHAR_HEIGHT;
        float y_base = (float)hud.layout.viewport_bottom + scroll_pixels;

        for (int i = 0; i < total; i++) {
            cg_chat_line_t *line = draw_lines[i].line;
            float y = y_base - (float)(total - 1 - i) * CONCHAR_HEIGHT;

            if (y < hud.layout.viewport_top - CONCHAR_HEIGHT ||
                y > hud.layout.viewport_bottom) {
                continue;
            }

            rgba_t color = base_color;
            color.a = static_cast<uint8_t>(Q_rint((float)color.a * draw_lines[i].alpha));

            int flags = 0;
            if (scr_chathud && scr_chathud->integer == 2 && line->is_chat)
                flags |= UI_ALTCOLOR;

            if (cgi.SCR_DrawString)
                cgi.SCR_DrawString(hud.layout.x, Q_rint(y), 1, flags,
                                   hud.layout.max_chars, line->text, color);
        }
    }

    if (message_active && cgi.CL_GetChatPrompt && cgi.CL_GetChatInput) {
        int prompt_skip = 0;
        const char *prompt = cgi.CL_GetChatPrompt(&prompt_skip);
        prompt_skip = CG_ChatHud_ClampPromptSkip(prompt_skip, hud.layout.max_chars);
        hud.layout.prompt_skip = prompt_skip;
        int prompt_width = cgi.SCR_MeasureString ? cgi.SCR_MeasureString(prompt, prompt_skip) : 0;

        int visible_chars = hud.layout.max_chars - prompt_skip;
        if (visible_chars < 1)
            visible_chars = 1;

        if (cgi.CL_SetChatVisibleChars)
            cgi.CL_SetChatVisibleChars((size_t)visible_chars);

        if (cgi.SCR_DrawString)
            cgi.SCR_DrawString(hud.layout.input_x, hud.layout.input_y, 1, 0,
                               hud.layout.max_chars, prompt, base_color);

        cg_chat_input_t input{};
        if (cgi.CL_GetChatInput(&input)) {
            if (input.visible_chars > 0) {
                CG_ChatHud_DrawInputField(input, hud.layout.input_x + prompt_width,
                                          hud.layout.input_y, UI_DRAWCURSOR,
                                          hud.layout.max_chars - prompt_skip, rgba_white);
            }
        }
    }

    if (message_active && total > max_visible) {
        float frac = hud.scroll_max > 0.0f ? (hud.scroll / hud.scroll_max) : 0.0f;
        int track_height = hud.layout.scroll_track_height;
        int thumb_height = max(6, Q_rint(track_height * ((float)max_visible / (float)total)));
        int travel = track_height - thumb_height;
        int thumb_top = hud.layout.scroll_track_top + Q_rint((1.0f - frac) * travel);

        hud.layout.thumb_top = thumb_top;
        hud.layout.thumb_height = thumb_height;

        rgba_t track = rgba_black;
        rgba_t thumb = rgba_white;
        track.a = (uint8_t)Q_rint(base_color.a * 0.35f);
        thumb.a = (uint8_t)Q_rint(base_color.a * 0.75f);

        cgi.SCR_DrawColorPic(hud.layout.scrollbar_x, hud.layout.scroll_track_top,
                             hud.layout.scrollbar_w, track_height, "_white", track);
        cgi.SCR_DrawColorPic(hud.layout.scrollbar_x, thumb_top,
                             hud.layout.scrollbar_w, thumb_height, "_white", thumb);
    }
}

/*
===============================================================================

CROSSHAIR + DAMAGE INDICATORS

===============================================================================
*/

constexpr uint64_t CROSSHAIR_PULSE_TIME_MS = 200;
constexpr float CROSSHAIR_PULSE_SMALL = 0.25f;
constexpr float CROSSHAIR_PULSE_LARGE = 0.5f;

constexpr int32_t MAX_DAMAGE_ENTRIES = 32;
constexpr int32_t DAMAGE_ENTRY_BASE_SIZE = 3;
constexpr int32_t MAX_TRACKED_POIS = 32;
constexpr float POI_NEAR_PLANE = 0.01f;

static const rgba_t ql_crosshair_colors[26] = {
    { 255, 0, 0, 255 },
    { 255, 64, 0, 255 },
    { 255, 128, 0, 255 },
    { 255, 192, 0, 255 },
    { 255, 255, 0, 255 },
    { 192, 255, 0, 255 },
    { 128, 255, 0, 255 },
    { 64, 255, 0, 255 },
    { 0, 255, 0, 255 },
    { 0, 255, 64, 255 },
    { 0, 255, 128, 255 },
    { 0, 255, 192, 255 },
    { 0, 255, 255, 255 },
    { 0, 192, 255, 255 },
    { 0, 128, 255, 255 },
    { 0, 64, 255, 255 },
    { 0, 0, 255, 255 },
    { 64, 0, 255, 255 },
    { 128, 0, 255, 255 },
    { 192, 0, 255, 255 },
    { 255, 0, 255, 255 },
    { 255, 0, 192, 255 },
    { 255, 0, 128, 255 },
    { 255, 0, 64, 255 },
    { 255, 255, 255, 255 },
    { 128, 128, 128, 255 }
};

static cvar_t *scr_crosshair;
static cvar_t *scr_hit_marker_time;
static cvar_t *scr_damage_indicators;
static cvar_t *scr_damage_indicator_time;
static cvar_t *cl_crosshair_brightness;
static cvar_t *cl_crosshair_color;
static cvar_t *cl_crosshair_health;
static cvar_t *cl_crosshair_hit_color;
static cvar_t *cl_crosshair_hit_style;
static cvar_t *cl_crosshair_hit_time;
static cvar_t *cl_crosshair_pulse;
static cvar_t *cl_crosshair_size;
static cvar_t *cl_hit_markers;
static cvar_t *ch_x;
static cvar_t *ch_y;
static cvar_t *scr_pois;
static cvar_t *scr_poi_edge_frac;
static cvar_t *scr_poi_max_scale;

struct cg_damage_entry_t {
    int damage = 0;
    Vector3 color{};
    Vector3 dir{};
    uint64_t time = 0;
};

struct cg_crosshair_state_t {
    int crosshair_index = 0;
    char crosshair_name[MAX_QPATH]{};
    int crosshair_raw_w = 0;
    int crosshair_raw_h = 0;
    int crosshair_width = 0;
    int crosshair_height = 0;
    rgba_t crosshair_color = rgba_white;
    uint64_t crosshair_pulse_time = 0;
    int last_pickup_icon = 0;
    int last_pickup_string = 0;
    uint64_t crosshair_hit_time = 0;
    int crosshair_hit_damage = 0;
    int hit_marker_count = 0;
    uint64_t hit_marker_time = 0;
    int hit_marker_frame = 0;
    int hit_marker_width = 0;
    int hit_marker_height = 0;
    int damage_display_width = 0;
    int damage_display_height = 0;
    std::array<cg_damage_entry_t, MAX_DAMAGE_ENTRIES> damage_entries{};
};

static std::array<cg_crosshair_state_t, MAX_SPLIT_PLAYERS> crosshair_state;

struct cg_poi_t {
    int id = 0;
    uint64_t time = 0;
    int color_index = 0;
    int flags = 0;
    int image_index = 0;
    char image_name[MAX_QPATH]{};
    int width = 0;
    int height = 0;
    Vector3 position{};
};

static std::array<std::array<cg_poi_t, MAX_TRACKED_POIS>, MAX_SPLIT_PLAYERS> poi_state;

static rgba_t CG_ApplyCrosshairBrightness(rgba_t color)
{
    float brightness = 1.0f;

    if (cl_crosshair_brightness) {
        brightness = Cvar_ClampValue(cl_crosshair_brightness, 0.0f, 1.0f);
    }

    color.r = Q_clip(Q_rint(color.r * brightness), 0, 255);
    color.g = Q_clip(Q_rint(color.g * brightness), 0, 255);
    color.b = Q_clip(Q_rint(color.b * brightness), 0, 255);

    return color;
}

static rgba_t CG_GetCrosshairPaletteColor(int index)
{
    index = Q_clip(index, 1, (int32_t)q_countof(ql_crosshair_colors));
    return ql_crosshair_colors[index - 1];
}

static rgba_t CG_GetCrosshairDamageColor(int damage)
{
    float t = Q_clipf((float)damage / 100.0f, 0.0f, 1.0f);

    return rgba_t{
        static_cast<uint8_t>(Q_rint(255.0f * t)),
        static_cast<uint8_t>(Q_rint(255.0f * (1.0f - t))),
        0,
        255
    };
}

static float CG_CalcPickupPulseScale(uint64_t start_time, uint64_t duration_ms)
{
    if (!start_time || !duration_ms)
        return 1.0f;

    uint64_t now = cgi.CL_ClientRealTime();
    if (now <= start_time)
        return 1.0f;

    uint64_t delta = now - start_time;
    if (delta >= duration_ms)
        return 1.0f;

    float frac = (float)delta / (float)duration_ms;
    return 1.0f + frac;
}

static float CG_CalcCrosshairPulseScale(uint64_t start_time, uint64_t duration_ms, float amplitude)
{
    if (!start_time || !duration_ms || amplitude <= 0.0f)
        return 1.0f;

    uint64_t now = cgi.CL_ClientRealTime();
    if (now <= start_time)
        return 1.0f;

    uint64_t delta = now - start_time;
    if (delta >= duration_ms)
        return 1.0f;

    float frac = (float)delta / (float)duration_ms;
    float falloff = 1.0f - (frac * frac);
    return 1.0f + (amplitude * falloff);
}

static void CG_UpdateHitMarkerSize(cg_crosshair_state_t &state)
{
    int w = 0;
    int h = 0;

    if (cgi.Draw_GetPicSize)
        cgi.Draw_GetPicSize(&w, &h, "marker");

    if (w < 1 || h < 1) {
        state.hit_marker_width = 0;
        state.hit_marker_height = 0;
        return;
    }

    float hit_marker_scale = 1.0f;
    if (cl_crosshair_size) {
        float size = Cvar_ClampValue(cl_crosshair_size, 1.0f, 512.0f);
        hit_marker_scale = Q_clipf(size / 32.0f, 0.1f, 9.0f);
    }

    state.hit_marker_width = Q_rint(w * hit_marker_scale);
    state.hit_marker_height = Q_rint(h * hit_marker_scale);
}

static void CG_UpdateCrosshairPic(cg_crosshair_state_t &state)
{
    int index = scr_crosshair ? scr_crosshair->integer : 0;
    if (index == state.crosshair_index)
        return;

    state.crosshair_index = index;
    state.crosshair_raw_w = 0;
    state.crosshair_raw_h = 0;

    if (index > 0) {
        std::snprintf(state.crosshair_name, sizeof(state.crosshair_name), "ch%i", index);
        if (cgi.Draw_GetPicSize)
            cgi.Draw_GetPicSize(&state.crosshair_raw_w, &state.crosshair_raw_h, state.crosshair_name);
    } else {
        state.crosshair_name[0] = '\0';
    }
}

static rgba_t CG_CalcCrosshairColor(const player_state_t *ps)
{
    rgba_t color = rgba_white;

    if (cl_crosshair_health && cl_crosshair_health->integer && ps) {
        int health = ps->stats[STAT_HEALTH];
        if (health <= 0) {
            color = rgba_t{ 0, 0, 0, 255 };
        } else {
            color.r = 255;

            if (health >= 66) {
                color.g = 255;
            } else if (health < 33) {
                color.g = 0;
            } else {
                color.g = static_cast<uint8_t>((255 * (health - 33)) / 33);
            }

            if (health >= 99) {
                color.b = 255;
            } else if (health < 66) {
                color.b = 0;
            } else {
                color.b = static_cast<uint8_t>((255 * (health - 66)) / 33);
            }
        }
    } else if (cl_crosshair_color) {
        int index = Cvar_ClampInteger(cl_crosshair_color, 1, (int32_t)q_countof(ql_crosshair_colors));
        color = CG_GetCrosshairPaletteColor(index);
    }

    color = CG_ApplyCrosshairBrightness(color);
    color.a = 255;
    return color;
}

void CG_NotifyPickupPulse(int32_t isplit)
{
    if (isplit < 0 || isplit >= (int32_t)crosshair_state.size())
        return;
    if (!cl_crosshair_pulse || !cl_crosshair_pulse->integer)
        return;

    crosshair_state[isplit].crosshair_pulse_time = cgi.CL_ClientRealTime();
}

static void CG_UpdateCrosshairPickupPulse(cg_crosshair_state_t &state, const player_state_t *ps, int32_t isplit)
{
    if (!ps)
        return;

    int pickup_icon = ps->stats[STAT_PICKUP_ICON];
    int pickup_string = ps->stats[STAT_PICKUP_STRING];

    if (pickup_icon != state.last_pickup_icon || pickup_string != state.last_pickup_string) {
        if (pickup_icon)
            CG_NotifyPickupPulse(isplit);
        state.last_pickup_icon = pickup_icon;
        state.last_pickup_string = pickup_string;
    }
}

void CG_NotifyHitMarker(int32_t isplit, int damage)
{
    if (isplit < 0 || isplit >= (int32_t)crosshair_state.size())
        return;
    if (damage <= 0)
        return;

    auto &state = crosshair_state[isplit];
    uint64_t now = cgi.CL_ClientRealTime();
    state.crosshair_hit_time = now;
    state.crosshair_hit_damage = damage;

    if (!cl_hit_markers || !cl_hit_markers->integer)
        return;

    int frame = cgi.CL_ServerFrame ? cgi.CL_ServerFrame() : 0;
    if (frame == state.hit_marker_frame)
        return;

    state.hit_marker_frame = frame;
    state.hit_marker_time = now;
    state.hit_marker_count++;
}

static void CG_DrawHitMarker(cg_crosshair_state_t &state, const rgba_t &base_color)
{
    if (!state.hit_marker_count)
        return;
    if (!scr_hit_marker_time || scr_hit_marker_time->integer <= 0) {
        state.hit_marker_count = 0;
        return;
    }

    uint64_t now = cgi.CL_ClientRealTime();
    uint64_t life = (uint64_t)scr_hit_marker_time->integer;
    if (now - state.hit_marker_time > life) {
        state.hit_marker_count = 0;
        return;
    }

    float frac = (float)(now - state.hit_marker_time) / (float)life;
    float alpha = 1.0f - (frac * frac);

    float scale = max(1.0f, 1.5f * (1.0f - frac));
    int w = Q_rint(state.hit_marker_width * scale);
    int h = Q_rint(state.hit_marker_height * scale);

    if (w < 1 || h < 1)
        return;

    cg_screen_metrics_t metrics{};
    if (!cgi.SCR_GetScreenMetrics)
        return;
    cgi.SCR_GetScreenMetrics(&metrics);

    int ui_scale = metrics.ui_scale > 0 ? metrics.ui_scale : 1;
    int offset_x = ch_x ? ch_x->integer : 0;
    int offset_y = ch_y ? ch_y->integer : 0;

    int x = (metrics.screen_width - w) / 2 + (offset_x * ui_scale);
    int y = (metrics.screen_height - h) / 2 + (offset_y * ui_scale);

    rgba_t color = rgba_t{ 255, 0, 0, static_cast<uint8_t>(Q_rint(base_color.a * alpha)) };

    if (cgi.SCR_DrawColorPic)
        cgi.SCR_DrawColorPic(x, y, w, h, "marker", color);
}

static cg_damage_entry_t *CG_AllocDamageDisplay(cg_crosshair_state_t &state, const Vector3 &dir)
{
    uint64_t now = cgi.CL_ClientRealTime();

    for (auto &entry : state.damage_entries) {
        if (entry.time <= now) {
            entry.damage = 0;
            entry.color = Vector3{};
            entry.dir = Vector3{};
            return &entry;
        }

        float dot = entry.dir.dot(dir);
        if (dot >= 0.95f) {
            return &entry;
        }
    }

    auto &entry = state.damage_entries[0];
    entry.damage = 0;
    entry.color = Vector3{};
    entry.dir = Vector3{};
    return &entry;
}

void CG_AddDamageDisplay(int32_t isplit, int damage, const Vector3 &color, const Vector3 &dir)
{
    if (isplit < 0 || isplit >= (int32_t)crosshair_state.size())
        return;
    if (!scr_damage_indicators || !scr_damage_indicators->integer)
        return;

    auto &state = crosshair_state[isplit];
    cg_damage_entry_t *entry = CG_AllocDamageDisplay(state, dir);

    entry->damage += damage;
    entry->color = (entry->color + color).normalized();
    entry->dir = dir;
    entry->time = cgi.CL_ClientRealTime() + (uint64_t)scr_damage_indicator_time->integer;
}

static void CG_DrawDamageDisplays(cg_crosshair_state_t &state, const rgba_t &base_color)
{
    if (!scr_damage_indicators || !scr_damage_indicators->integer)
        return;

    uint64_t now = cgi.CL_ClientRealTime();
    if (!scr_damage_indicator_time || scr_damage_indicator_time->value <= 0.0f)
        return;

    cg_view_params_t view{};
    if (!cgi.CL_GetViewParams || !cgi.SCR_GetScreenMetrics)
        return;
    cgi.CL_GetViewParams(&view);

    cg_screen_metrics_t metrics{};
    cgi.SCR_GetScreenMetrics(&metrics);

    float my_yaw = view.viewangles.y;

    for (auto &entry : state.damage_entries) {
        if (entry.time <= now)
            continue;

        float frac = (float)(entry.time - now) / scr_damage_indicator_time->value;
        Vector3 angles = VectorToAngles(entry.dir);
        float damage_yaw = angles.y;
        float yaw_diff = DEG2RAD((my_yaw - damage_yaw) - 180.0f);

        rgba_t color = rgba_white;
        color.r = static_cast<uint8_t>(Q_rint(entry.color.x * 255.0f));
        color.g = static_cast<uint8_t>(Q_rint(entry.color.y * 255.0f));
        color.b = static_cast<uint8_t>(Q_rint(entry.color.z * 255.0f));
        color.a = static_cast<uint8_t>(Q_rint(frac * base_color.a));

        int x = metrics.hud_width / 2;
        int y = metrics.hud_height / 2;

        int size = min(state.damage_display_width, DAMAGE_ENTRY_BASE_SIZE * entry.damage);

        if (cgi.SCR_DrawStretchRotatePic) {
            cgi.SCR_DrawStretchRotatePic(x, y, size, state.damage_display_height,
                                         color, yaw_diff, 0,
                                         -(state.crosshair_height + (state.damage_display_height / 2)),
                                         "damage_indicator");
        }
    }
}

static void CG_SetPOIImage(cg_poi_t &poi, int image_index)
{
    poi.image_index = image_index;
    poi.image_name[0] = '\0';
    poi.width = 0;
    poi.height = 0;

    if (!cgi.CL_GetImageConfigString || !cgi.Draw_GetPicSize)
        return;

    const char *name = cgi.CL_GetImageConfigString(image_index);
    if (!name || !*name)
        return;

    Q_strlcpy(poi.image_name, name, sizeof(poi.image_name));
    cgi.Draw_GetPicSize(&poi.width, &poi.height, poi.image_name);
}

void CG_RemovePOI(int32_t isplit, int id)
{
    if (isplit < 0 || isplit >= (int32_t)poi_state.size())
        return;
    if (!scr_pois || !scr_pois->integer)
        return;

    if (id == 0) {
        if (cgi.Com_Print)
            cgi.Com_Print("tried to remove unkeyed POI\n");
        return;
    }

    auto &pois = poi_state[isplit];
    for (auto &poi : pois) {
        if (poi.id == id) {
            poi = {};
            break;
        }
    }
}

void CG_AddPOI(int32_t isplit, int id, int time, const Vector3 &pos, int image, int color, int flags)
{
    if (isplit < 0 || isplit >= (int32_t)poi_state.size())
        return;
    if (!scr_pois || !scr_pois->integer)
        return;
    if (!cgi.CL_ClientTime)
        return;

    uint64_t now = cgi.CL_ClientTime();
    auto &pois = poi_state[isplit];
    cg_poi_t *poi = nullptr;

    if (id == 0) {
        cg_poi_t *oldest_poi = nullptr;

        for (auto &candidate : pois) {
            if (candidate.time > now) {
                if (candidate.id) {
                    continue;
                }
                if (!oldest_poi || candidate.time < oldest_poi->time) {
                    oldest_poi = &candidate;
                }
            } else {
                poi = &candidate;
                break;
            }
        }

        if (!poi)
            poi = oldest_poi;
    } else {
        cg_poi_t *oldest_poi = nullptr;
        cg_poi_t *free_poi = nullptr;

        for (auto &candidate : pois) {
            if (candidate.id == id) {
                poi = &candidate;
                break;
            }

            if (candidate.time <= now) {
                if (!free_poi) {
                    free_poi = &candidate;
                }
            } else if (!candidate.id) {
                if (!oldest_poi || candidate.time < oldest_poi->time) {
                    oldest_poi = &candidate;
                }
            }
        }

        if (!poi)
            poi = free_poi ? free_poi : oldest_poi;
    }

    if (!poi) {
        if (cgi.Com_Print)
            cgi.Com_Print("couldn't add a POI\n");
        return;
    }

    poi->id = id;
    poi->time = now + static_cast<uint64_t>(time);
    poi->position = pos;
    poi->color_index = color;
    poi->flags = flags;
    CG_SetPOIImage(*poi, image);
}

static void CG_DrawPOIs(int32_t isplit)
{
    if (isplit < 0 || isplit >= (int32_t)poi_state.size())
        return;
    if (!scr_pois || !scr_pois->integer)
        return;
    if (!cgi.CL_ClientTime || !cgi.CL_GetViewParams || !cgi.SCR_GetScreenMetrics || !cgi.SCR_DrawColorPic)
        return;

    cg_view_params_t view{};
    cgi.CL_GetViewParams(&view);

    cg_screen_metrics_t metrics{};
    cgi.SCR_GetScreenMetrics(&metrics);

    float tan_x = tanf(DEG2RAD(view.fov_x * 0.5f));
    float tan_y = tanf(DEG2RAD(view.fov_y * 0.5f));
    if (tan_x <= 0.0f || tan_y <= 0.0f)
        return;

    if (cgi.SCR_SetScale)
        cgi.SCR_SetScale(metrics.hud_scale);

    Vector3 forward{};
    Vector3 right{};
    Vector3 up{};
    AngleVectors(view.viewangles, forward, right, up);

    uint64_t now = cgi.CL_ClientTime();
    float max_height = metrics.hud_height * 0.75f;

    uint8_t base_alpha = 255;
    if (scr_alpha)
        base_alpha = static_cast<uint8_t>(Q_rint(255.0f * Cvar_ClampValue(scr_alpha, 0.0f, 1.0f)));

    auto &pois = poi_state[isplit];
    for (auto &poi : pois) {
        if (poi.time <= now)
            continue;
        if (!poi.image_name[0] || poi.width <= 0 || poi.height <= 0)
            continue;

        Vector3 local = poi.position - view.vieworg;
        float x = local.dot(right);
        float y = local.dot(up);
        float z = local.dot(forward);
        bool behind = z < 0.0f;

        if (z > -POI_NEAR_PLANE && z < POI_NEAR_PLANE)
            z = behind ? -POI_NEAR_PLANE : POI_NEAR_PLANE;

        float inv_z = 1.0f / z;
        float ndc_x = (x * inv_z) / tan_x;
        float ndc_y = (y * inv_z) / tan_y;

        float screen_x = ((ndc_x * 0.5f) + 0.5f) * metrics.hud_width;
        float screen_y = ((-ndc_y * 0.5f) + 0.5f) * metrics.hud_height;

        if (behind) {
            screen_x = metrics.hud_width - screen_x;
            screen_y = metrics.hud_height - screen_y;

            if (screen_y > 0.0f) {
                if (screen_x < (metrics.hud_width / 2.0f))
                    screen_x = 0.0f;
                else
                    screen_x = (float)(metrics.hud_width - 1);

                screen_y = min(screen_y, max_height);
            }
        }

        float scale = 1.0f;
        if (scr_poi_max_scale && scr_poi_edge_frac && scr_poi_max_scale->value != 1.0f) {
            float edge_dist = min(metrics.hud_width, metrics.hud_height) * scr_poi_edge_frac->value;

            for (int axis = 0; axis < 2; ++axis) {
                float extent = (axis == 0) ? (float)metrics.hud_width : (float)metrics.hud_height;
                float coord = (axis == 0) ? screen_x : screen_y;
                float frac;

                if (coord < edge_dist) {
                    frac = coord / edge_dist;
                } else if (coord > extent - edge_dist) {
                    frac = (extent - coord) / edge_dist;
                } else {
                    continue;
                }

                scale = Q_clipf(1.0f + (1.0f - frac) * (scr_poi_max_scale->value - 1.0f),
                                scale, scr_poi_max_scale->value);
            }
        }

        int hw = (int)((poi.width * scale) / 2.0f);
        int hh = (int)((poi.height * scale) / 2.0f);
        if (hw <= 0 || hh <= 0)
            continue;

        screen_x -= hw;
        screen_y -= hh;

        screen_x = Q_clipf(screen_x, 0.0f, (float)(metrics.hud_width - hw));
        screen_y = Q_clipf(screen_y, 0.0f, (float)(metrics.hud_height - hh));

        rgba_t color = cgi.CL_GetPaletteColor ? cgi.CL_GetPaletteColor(poi.color_index) : rgba_white;

        if (poi.flags & POI_FLAG_HIDE_ON_AIM) {
            Vector3 centered{ (metrics.hud_width / 2.0f) - screen_x, (metrics.hud_height / 2.0f) - screen_y, 0.0f };
            float len = centered.length();
            float alpha_scale = Q_clipf(len / (hw * 6.0f), 0.25f, 1.0f);
            color.a = static_cast<uint8_t>(Q_rint(base_alpha * alpha_scale));
        } else {
            color.a = base_alpha;
        }

        cgi.SCR_DrawColorPic(static_cast<int>(screen_x), static_cast<int>(screen_y), hw, hh, poi.image_name, color);
    }
}

void CG_DrawCrosshair(int32_t isplit, const player_state_t *ps)
{
    if (isplit < 0 || isplit >= (int32_t)crosshair_state.size())
        return;

    if (!scr_crosshair || scr_crosshair->integer <= 0)
        return;
    if (!ps)
        return;
    if (ps->stats[STAT_LAYOUTS] & (LAYOUTS_HIDE_HUD | LAYOUTS_HIDE_CROSSHAIR))
        return;

    auto &state = crosshair_state[isplit];
    CG_DrawPOIs(isplit);
    CG_UpdateCrosshairPickupPulse(state, ps, isplit);
    CG_UpdateCrosshairPic(state);

    if (cl_crosshair_size)
        CG_UpdateHitMarkerSize(state);

    if (state.damage_display_width <= 0 || state.damage_display_height <= 0) {
        if (cgi.Draw_GetPicSize)
            cgi.Draw_GetPicSize(&state.damage_display_width, &state.damage_display_height, "damage_indicator");
    }

    if (!state.crosshair_name[0])
        return;

    int raw_w = state.crosshair_raw_w;
    int raw_h = state.crosshair_raw_h;
    if (raw_w < 1 || raw_h < 1) {
        if (cgi.Draw_GetPicSize)
            cgi.Draw_GetPicSize(&raw_w, &raw_h, state.crosshair_name);
        state.crosshair_raw_w = raw_w;
        state.crosshair_raw_h = raw_h;
    }

    if (raw_w < 1 || raw_h < 1)
        return;

    cg_screen_metrics_t metrics{};
    if (!cgi.SCR_GetScreenMetrics)
        return;
    cgi.SCR_GetScreenMetrics(&metrics);

    int ui_scale = metrics.ui_scale > 0 ? metrics.ui_scale : 1;
    float crosshair_size = cl_crosshair_size ? Cvar_ClampValue(cl_crosshair_size, 1.0f, 512.0f) : 32.0f;
    int max_dim = max(raw_w, raw_h);
    float scale = max_dim > 0 ? (crosshair_size / (float)max_dim) : 1.0f;

    int base_w = Q_rint((float)raw_w * ui_scale * scale);
    int base_h = Q_rint((float)raw_h * ui_scale * scale);
    if (base_w < 1)
        base_w = 1;
    if (base_h < 1)
        base_h = 1;

    state.crosshair_width = max(1, base_w / ui_scale);
    state.crosshair_height = max(1, base_h / ui_scale);

    int hit_style = cl_crosshair_hit_style ? Cvar_ClampInteger(cl_crosshair_hit_style, 0, 8) : 0;
    int hit_time = cl_crosshair_hit_time ? Cvar_ClampInteger(cl_crosshair_hit_time, 0, 10000) : 0;
    bool hit_active = false;

    if (hit_style > 0 && hit_time > 0 && state.crosshair_hit_time) {
        uint64_t delta = cgi.CL_ClientRealTime() - state.crosshair_hit_time;
        if (delta <= (uint64_t)hit_time)
            hit_active = true;
    }

    float pulse_scale = 1.0f;
    if (cl_crosshair_pulse && cl_crosshair_pulse->integer) {
        pulse_scale = max(pulse_scale, CG_CalcPickupPulseScale(
            state.crosshair_pulse_time, CROSSHAIR_PULSE_TIME_MS));
    }

    if (hit_active && (hit_style == 3 || hit_style == 4 || hit_style == 5 ||
                       hit_style == 6 || hit_style == 7 || hit_style == 8)) {
        float amplitude = (hit_style >= 6) ? CROSSHAIR_PULSE_SMALL : CROSSHAIR_PULSE_LARGE;
        pulse_scale = max(pulse_scale, CG_CalcCrosshairPulseScale(
            state.crosshair_hit_time, (uint64_t)hit_time, amplitude));
    }

    int w = Q_rint(base_w * pulse_scale);
    int h = Q_rint(base_h * pulse_scale);
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;

    int offset_x = ch_x ? ch_x->integer : 0;
    int offset_y = ch_y ? ch_y->integer : 0;
    int x = (metrics.screen_width - w) / 2 + (offset_x * ui_scale);
    int y = (metrics.screen_height - h) / 2 + (offset_y * ui_scale);

    rgba_t crosshair_color = CG_CalcCrosshairColor(ps);
    if (hit_active) {
        rgba_t hit_color{};
        bool override_color = false;

        if (hit_style == 1 || hit_style == 4 || hit_style == 7) {
            hit_color = CG_GetCrosshairDamageColor(state.crosshair_hit_damage);
            override_color = true;
        } else if (hit_style == 2 || hit_style == 5 || hit_style == 8) {
            int index = cl_crosshair_hit_color ? Cvar_ClampInteger(
                cl_crosshair_hit_color, 1, (int32_t)q_countof(ql_crosshair_colors)) : 1;
            hit_color = CG_GetCrosshairPaletteColor(index);
            override_color = true;
        }

        if (override_color) {
            crosshair_color = CG_ApplyCrosshairBrightness(hit_color);
        }
    }

    if (scr_alpha)
        crosshair_color.a = static_cast<uint8_t>(Q_rint(crosshair_color.a * Cvar_ClampValue(scr_alpha, 0, 1)));

    if (cgi.SCR_SetScale)
        cgi.SCR_SetScale((float)metrics.base_scale);
    if (cgi.SCR_DrawColorPic)
        cgi.SCR_DrawColorPic(x, y, w, h, state.crosshair_name, crosshair_color);
    if (cgi.SCR_SetScale)
        cgi.SCR_SetScale(metrics.hud_scale);

    CG_DrawHitMarker(state, crosshair_color);
    CG_DrawDamageDisplays(state, crosshair_color);
}

static inline bool CG_IsColorEscapeCode(char c)
{
    if (c >= '0' && c <= '7')
        return true;
    if (c >= 'a' && c <= 'z')
        return true;
    if (c >= 'A' && c <= 'Z')
        return true;
    return false;
}

static bool CG_ParseColorEscape(const char **text, size_t *remaining, const rgba_t &base, rgba_t *out_color)
{
    if (!text || !*text || !remaining)
        return false;
    if (*remaining < 2)
        return false;
    if (**text != '^')
        return false;
    if (!CG_IsColorEscapeCode((*text)[1]))
        return false;

    rgba_t parsed{};
    if ((*text)[1] >= '0' && (*text)[1] <= '7') {
        parsed = q3_color_table[(*text)[1] - '0'];
    } else if ((*text)[1] >= 'a' && (*text)[1] <= 'z') {
        parsed = q3_rainbow_colors[(*text)[1] - 'a'];
    } else if ((*text)[1] >= 'A' && (*text)[1] <= 'Z') {
        parsed = q3_rainbow_colors[(*text)[1] - 'A'];
    } else {
        return false;
    }

    parsed.a = base.a;
    *text += 2;
    *remaining -= 2;
    if (out_color)
        *out_color = parsed;
    return true;
}

static bool CG_HasColorEscape(const char *text, size_t maxlen)
{
    if (!text || !*text)
        return false;

    size_t remaining = maxlen;
    const char *s = text;
    while (remaining && *s) {
        if (remaining >= 2 && *s == '^' && CG_IsColorEscapeCode(s[1]))
            return true;
        ++s;
        --remaining;
    }

    return false;
}

static size_t CG_StrlenNoColor(const char *text, size_t maxlen)
{
    if (!text || !*text)
        return 0;

    size_t remaining = maxlen;
    size_t len = 0;
    const char *s = text;
    while (remaining && *s) {
        if (remaining >= 2 && *s == '^' && CG_IsColorEscapeCode(s[1])) {
            s += 2;
            remaining -= 2;
            continue;
        }
        ++s;
        --remaining;
        ++len;
    }

    return len;
}

static int CG_DrawStringColored(int x, int y, int scale, const char *text, const rgba_t &base_color, bool shadow)
{
    if (!text || !*text)
        return x;

    size_t remaining = strlen(text);
    bool use_color_codes = CG_HasColorEscape(text, remaining);
    rgba_t draw_color = base_color;
    int flags = shadow ? UI_DROPSHADOW : 0;

    while (remaining && *text) {
        if (use_color_codes) {
            rgba_t parsed;
            if (CG_ParseColorEscape(&text, &remaining, base_color, &parsed)) {
                draw_color = parsed;
                continue;
            }
        }

        char c = *text++;
        remaining--;
        cgi.SCR_DrawCharStretch(x, y, CONCHAR_WIDTH * scale, CONCHAR_WIDTH * scale,
                                flags, c, draw_color);
        x += CONCHAR_WIDTH * scale;
    }

    return x;
}

/*
==============
CG_DrawHUDString
==============
*/
static int CG_DrawHUDString (const char *string, int x, int y, int centerwidth, int _xor, int scale, bool shadow = true)
{
    int     margin;
    char    line[1024];
    int     width;
    margin = x;

    while (*string)
    {
        // scan out one line of text from the string
        width = 0;
        while (*string && *string != '\n')
            line[width++] = *string++;
        line[width] = 0;

        vec2_t size;
        
        if (scr_usekfont->integer)
            size = cgi.SCR_MeasureFontString(line, scale);
        int visible_width = width;
        if (!scr_usekfont->integer)
            visible_width = static_cast<int>(CG_StrlenNoColor(line, static_cast<size_t>(width)));

        if (centerwidth)
        {
            if (!scr_usekfont->integer)
                x = margin + ((centerwidth - visible_width*CONCHAR_WIDTH*scale))/2;
            else
                x = margin + ((centerwidth - size.x))/2;
        }
        else
            x = margin;

        if (!scr_usekfont->integer)
        {
            rgba_t base_color = _xor ? alt_color : rgba_white;
            CG_DrawStringColored(x, y, scale, line, base_color, shadow);
        }
        else
        {
            cgi.SCR_DrawFontString(line, x, y - (font_y_offset * scale), scale, _xor ? alt_color : rgba_white, true, text_align_t::LEFT);
            x += size.x;
        }

        if (*string)
        {
            string++;   // skip the \n
            x = margin;
            if (!scr_usekfont->integer)
                y += CONCHAR_WIDTH * scale;
            else
                // TODO
                y += 10 * scale;//size.y;
        }
    }

    return x;
}

// Shamefully stolen from Kex
size_t FindStartOfUTF8Codepoint(const std::string &str, size_t pos)
{
    if(pos >= str.size())
    {
        return std::string::npos;
    }

    for(ptrdiff_t i = pos; i >= 0; i--)
    {
        const char &ch = str[i];

        if((ch & 0x80) == 0)
        {
            // character is one byte
            return i;
        }
        else if((ch & 0xC0) == 0x80)
        {
            // character is part of a multi-byte sequence, keep going
            continue;
        }
        else
        {
            // character is the start of a multi-byte sequence, so stop now
            return i;
        }
    }

    return std::string::npos;
}

size_t FindEndOfUTF8Codepoint(const std::string &str, size_t pos)
{
    if(pos >= str.size())
    {
        return std::string::npos;
    }

    for(size_t i = pos; i < str.size(); i++)
    {
        const char &ch = str[i];

        if((ch & 0x80) == 0)
        {
            // character is one byte
            return i;
        }
        else if((ch & 0xC0) == 0x80)
        {
            // character is part of a multi-byte sequence, keep going
            continue;
        }
        else
        {
            // character is the start of a multi-byte sequence, so stop now
            return i;
        }
    }

    return std::string::npos;
}

void CG_NotifyMessage(int32_t isplit, const char *msg, bool is_chat)
{
    const char *visible = msg ? CG_SkipObituaryMetadata(msg) : "";
    const bool has_meta = msg && visible != msg;

    bool is_obituary = false;
    if (!is_chat) {
        cl_obituary_t obit;
        if (has_meta && CG_ParseObituaryMetadata(msg, obit)) {
            CG_AddObituary(hud_data[isplit], obit);
            is_obituary = true;
        } else if (CG_ParseObituaryMessage(visible, obit)) {
            CG_AddObituary(hud_data[isplit], obit);
            is_obituary = true;
        }
    }

    if (!is_obituary)
        CG_AddNotify(hud_data[isplit], visible, is_chat);
}

// centerprint stuff
static cl_centerprint_t &CG_QueueCenterPrint(int isplit, bool instant)
{
    auto &icl = hud_data[isplit];

    // just use first index
    if (!icl.center_index.has_value() || instant)
    {
        icl.center_index = 0;

        for (size_t i = 1; i < MAX_CENTER_PRINTS; i++)
            icl.centers[i].lines.clear();

        return icl.centers[0];
    }

    // pick the next free index if we can find one
    for (size_t i = 1; i < MAX_CENTER_PRINTS; i++)
    {
        auto &center = icl.centers[(icl.center_index.value() + i) % MAX_CENTER_PRINTS];

        if (center.lines.empty())
            return center;
    }
    
    // none, so update the current one (the new end of buffer)
    // and skip ahead
    auto &center = icl.centers[icl.center_index.value()];
    icl.center_index = (icl.center_index.value() + 1) % MAX_CENTER_PRINTS;
    return center;
}

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void CG_ParseCenterPrint (const char *str, int isplit, bool instant) // [Sam-KEX] Made 1st param const
{
    const char    *s;
    char    line[64];
    int     i, j, l;

    // handle center queueing
    cl_centerprint_t &center = CG_QueueCenterPrint(isplit, instant);

    center.lines.clear();

    // split the string into lines
    size_t line_start = 0;

    std::string string(str);

    center.binds.clear();

    // [Paril-KEX] pull out bindings. they'll always be at the start
    while (string.compare(0, 6, "%bind:") == 0)
    {
        size_t end_of_bind = string.find_first_of('%', 1);

        if (end_of_bind == std::string::npos)
            break;

        std::string bind = string.substr(6, end_of_bind - 6);

        if (auto purpose_index = bind.find_first_of(':'); purpose_index != std::string::npos)
            center.binds.emplace_back(cl_bind_t { bind.substr(0, purpose_index), bind.substr(purpose_index + 1) });
        else
            center.binds.emplace_back(cl_bind_t { bind });

        string = string.substr(end_of_bind + 1);
    }

    // echo it to the console
    cgi.Com_Print("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");

    s = string.c_str();
    do
    {
        // scan the width of the line
        for (l=0 ; l<40 ; l++)
            if (s[l] == '\n' || !s[l])
                break;
        for (i=0 ; i<(40-l)/2 ; i++)
            line[i] = ' ';

        for (j=0 ; j<l ; j++)
        {
            line[i++] = s[j];
        }

        line[i] = '\n';
        line[i+1] = 0;

        cgi.Com_Print(line);

        while (*s && *s != '\n')
            s++;

        if (!*s)
            break;
        s++;        // skip the \n
    } while (1);
    cgi.Com_Print("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
    CG_ClearNotify (isplit);

    for (size_t line_end = 0; ; )
    {
        line_end = FindEndOfUTF8Codepoint(string, line_end);

        if (line_end == std::string::npos)
        {
            // final line
            if (line_start < string.size())
                center.lines.emplace_back(string.c_str() + line_start);
            break;
        }
        
        // char part of current line;
        // if newline, end line and cut off
        const char &ch = string[line_end];

        if (ch == '\n')
        {
            if (line_end > line_start)
                center.lines.emplace_back(string.c_str() + line_start, line_end - line_start);
            else
                center.lines.emplace_back();
            line_start = line_end + 1;
            line_end++;
            continue;
        }

         line_end++;
    }

    if (center.lines.empty())
    {
        center.finished = true;
        return;
    }

    center.time_tick = cgi.CL_ClientRealTime() + (scr_printspeed->value * 1000);
    center.instant = instant;
    center.finished = false;
    center.current_line = 0;
    center.line_count = 0;
}

static void CG_DrawCenterString( const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int isplit, int scale, cl_centerprint_t &center)
{
    int32_t y = hud_vrect.y * scale;
    
    if (CG_ViewingLayout(ps))
        y += hud_safe.y;
    else if (center.lines.size() <= 4)
        y += (hud_vrect.height * 0.2f) * scale;
    else
        y += 48 * scale;

    int lineHeight = (scr_usekfont->integer ? 10 : 8) * scale;
    if (ui_acc_alttypeface->integer) lineHeight *= 1.5f;

    // easy!
    if (center.instant)
    {
        for (size_t i = 0; i < center.lines.size(); i++)
        {
            auto &line = center.lines[i];

            cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);

            if (ui_acc_contrast->integer && line.length())
            {
                vec2_t sz = cgi.SCR_MeasureFontString(line.c_str(), scale);
                sz.x += 10; // extra padding for black bars
                int barY = ui_acc_alttypeface->integer ? y - 8 : y;
                cgi.SCR_DrawColorPic((hud_vrect.x + hud_vrect.width / 2) * scale - (sz.x / 2), barY, sz.x, lineHeight, "_white", rgba_black);
            }
            CG_DrawHUDString(line.c_str(), (hud_vrect.x + hud_vrect.width/2 + -160) * scale, y, (320 / 2) * 2 * scale, 0, scale);

            cgi.SCR_SetAltTypeface(false);

            y += lineHeight;
        }

        for (auto &bind : center.binds)
        {
            y += lineHeight * 2;
            cgi.SCR_DrawBind(isplit, bind.bind.c_str(), bind.purpose.c_str(), (hud_vrect.x + (hud_vrect.width / 2)) * scale, y, scale);
        }

        if (!center.finished)
        {
            center.finished = true;
            center.time_off = cgi.CL_ClientRealTime() + (scr_centertime->value * 1000);
        }

        return;
    }

    // hard and annoying!
    // check if it's time to fetch a new char
    const uint64_t t = cgi.CL_ClientRealTime();

    if (!center.finished)
    {
        if (center.time_tick < t)
        {
            center.time_tick = t + (scr_printspeed->value * 1000);
            center.line_count = FindEndOfUTF8Codepoint(center.lines[center.current_line], center.line_count + 1);

            if (center.line_count == std::string::npos)
            {
                center.current_line++;
                center.line_count = 0;

                if (center.current_line == center.lines.size())
                {
                    center.current_line--;
                    center.finished = true;
                    center.time_off = t + (scr_centertime->value * 1000);
                }
            }
        }
    }

    // smallish byte buffer for single line of data...
    char buffer[256];

    for (size_t i = 0; i < center.lines.size(); i++)
    {
        cgi.SCR_SetAltTypeface(ui_acc_alttypeface->integer && true);

        auto &line = center.lines[i];

        buffer[0] = 0;

        if (center.finished || i != center.current_line)
            Q_strlcpy(buffer, line.c_str(), sizeof(buffer));
        else
            Q_strlcpy(buffer, line.c_str(), min(center.line_count + 1, sizeof(buffer)));

        int blinky_x;

        if (ui_acc_contrast->integer && line.length())
        {
            vec2_t sz = cgi.SCR_MeasureFontString(line.c_str(), scale);
            sz.x += 10; // extra padding for black bars
            int barY = ui_acc_alttypeface->integer ? y - 8 : y;
            cgi.SCR_DrawColorPic((hud_vrect.x + hud_vrect.width / 2) * scale - (sz.x / 2), barY, sz.x, lineHeight, "_white", rgba_black);
        }
        
        if (buffer[0])
            blinky_x = CG_DrawHUDString(buffer, (hud_vrect.x + hud_vrect.width/2 + -160) * scale, y, (320 / 2) * 2 * scale, 0, scale);
        else
            blinky_x = (hud_vrect.width / 2) * scale;

        cgi.SCR_SetAltTypeface(false);

        if (i == center.current_line && !ui_acc_alttypeface->integer)
            cgi.SCR_DrawChar(blinky_x, y, scale, 10 + ((cgi.CL_ClientRealTime() >> 8) & 1), true);

        y += lineHeight;

        if (i == center.current_line)
            break;
    }
}

static void CG_CheckDrawCenterString( const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int isplit, int scale )
{
    if (CG_InIntermission(ps))
        return;
    if (!hud_data[isplit].center_index.has_value())
        return;

    auto &data = hud_data[isplit];
    auto &center = data.centers[data.center_index.value()];

    // ran out of center time
    if (center.finished && center.time_off < cgi.CL_ClientRealTime())
    {
        center.lines.clear();

        size_t next_index = (data.center_index.value() + 1) % MAX_CENTER_PRINTS;
        auto &next_center = data.centers[next_index];

        // no more
        if (next_center.lines.empty())
        {
            data.center_index.reset();
            return;
        }

        // buffer rotated; start timer now
        data.center_index = next_index;
        next_center.current_line = next_center.line_count = 0;
    }

    if (!data.center_index.has_value())
        return;

    CG_DrawCenterString( ps, hud_vrect, hud_safe, isplit, scale, data.centers[data.center_index.value()] );
}

/*
==============
CG_DrawString
==============
*/
static void CG_DrawString (int x, int y, int scale, const char *s, bool alt = false, bool shadow = true)
{
    rgba_t base_color = alt ? alt_color : rgba_white;
    CG_DrawStringColored(x, y, scale, s, base_color, shadow);
}

#include <charconv>

/*
==============
CG_DrawField
==============
*/
static void CG_DrawField (int x, int y, int color, int width, int value, int scale)
{
    char    num[16], *ptr;
    int     l;
    int     frame;

    if (width < 1)
        return;

    // draw number string
    if (width > 5)
        width = 5;

    auto result = std::to_chars(num, num + sizeof(num) - 1, value);
    *(result.ptr) = '\0';

    l = (result.ptr - num);

    if (l > width)
        l = width;

    x += (2 + CHAR_WIDTH*(width - l)) * scale;

    ptr = num;
    while (*ptr && l)
    {
        if (*ptr == '-')
            frame = STAT_MINUS;
        else
            frame = *ptr -'0';
        int w, h;
        cgi.Draw_GetPicSize(&w, &h, sb_nums[color][frame]);
        cgi.SCR_DrawPic(x, y, w * scale, h * scale, sb_nums[color][frame]);
        x += CHAR_WIDTH * scale;
        ptr++;
        l--;
    }
}

// [Paril-KEX]
static void CG_DrawTable(int x, int y, uint32_t width, uint32_t height, int32_t scale)
{
    // half left
    int32_t width_pixels = width;
    x -= width_pixels / 2;
    y += CONCHAR_WIDTH * scale;
    // use Y as top though

    int32_t height_pixels = height;
    
    // draw border
    // KEX_FIXME method that requires less chars
    cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), y - (CONCHAR_WIDTH * scale), scale, 18, false);
    cgi.SCR_DrawChar((x + width_pixels), y - (CONCHAR_WIDTH * scale), scale, 20, false);
    cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), y + height_pixels, scale, 24, false);
    cgi.SCR_DrawChar((x + width_pixels), y + height_pixels, scale, 26, false);

    for (int cx = x; cx < x + width_pixels; cx += CONCHAR_WIDTH * scale)
    {
        cgi.SCR_DrawChar(cx, y - (CONCHAR_WIDTH * scale), scale, 19, false);
        cgi.SCR_DrawChar(cx, y + height_pixels, scale, 25, false);
    }

    for (int cy = y; cy < y + height_pixels; cy += CONCHAR_WIDTH * scale)
    {
        cgi.SCR_DrawChar(x - (CONCHAR_WIDTH * scale), cy, scale, 21, false);
        cgi.SCR_DrawChar((x + width_pixels), cy, scale, 23, false);
    }

    cgi.SCR_DrawColorPic(x, y, width_pixels, height_pixels, "_white", { 0, 0, 0, 255 });

    // draw in columns
    for (int i = 0; i < hud_temp.num_columns; i++)
    {
        for (int r = 0, ry = y; r < hud_temp.num_rows; r++, ry += (CONCHAR_WIDTH + font_y_offset) * scale)
        {
            int x_offset = 0;

            // center 
            if (r == 0)
            {
                x_offset = ((hud_temp.column_widths[i]) / 2) -
                    ((cgi.SCR_MeasureFontString(hud_temp.table_rows[r].table_cells[i].text, scale).x) / 2);
            }
            // right align
            else if (i != 0)
            {
                x_offset = (hud_temp.column_widths[i] - cgi.SCR_MeasureFontString(hud_temp.table_rows[r].table_cells[i].text, scale).x);
            }

            //CG_DrawString(x + x_offset, ry, scale, hud_temp.table_rows[r].table_cells[i].text, r == 0, true);
            cgi.SCR_DrawFontString(hud_temp.table_rows[r].table_cells[i].text, x + x_offset, ry - (font_y_offset * scale), scale, r == 0 ? alt_color : rgba_white, true, text_align_t::LEFT);
        }

        x += (hud_temp.column_widths[i] + cgi.SCR_MeasureFontString(" ", 1).x);
    }
}

/*
================
CG_ExecuteLayoutString

================
*/
static void CG_ExecuteLayoutString (const char *s, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps)
{
    int     x, y;
    int     w, h;
    int     hx, hy;
    int     value;
    const char *token;
    int     width;
    int     index;

    if (!s[0])
        return;

    x = hud_vrect.x;
    y = hud_vrect.y;
    width = 3;

    hx = 320 / 2;
    hy = 240 / 2;

    bool flash_frame = (cgi.CL_ClientTime() % 1000) < 500;

    // if non-zero, parse but don't affect state
    int32_t if_depth = 0; // current if statement depth
    int32_t endif_depth = 0; // at this depth, toggle skip_depth
    bool skip_depth = false; // whether we're in a dead stmt or not

    while (s)
    {
        token = COM_Parse (&s);
        if (!strcmp(token, "xl"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                x = ((hud_vrect.x + atoi(token)) * scale) + hud_safe.x;
            continue;
        }
        if (!strcmp(token, "xr"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                x = ((hud_vrect.x + hud_vrect.width + atoi(token)) * scale) - hud_safe.x;
            continue;
        }
        if (!strcmp(token, "xv"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                x = (hud_vrect.x + hud_vrect.width/2 + (atoi(token) - hx)) * scale;
            continue;
        }

        if (!strcmp(token, "yt"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                y = ((hud_vrect.y + atoi(token)) * scale) + hud_safe.y;
            continue;
        }
        if (!strcmp(token, "yb"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                y = ((hud_vrect.y + hud_vrect.height + atoi(token)) * scale) - hud_safe.y;
            continue;
        }
        if (!strcmp(token, "yv"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                y = (hud_vrect.y + hud_vrect.height/2 + (atoi(token) - hy)) * scale;
            continue;
        }

        if (!strcmp(token, "pic"))
        {   // draw a pic from a stat number
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = ps->stats[atoi(token)];
                if (value >= MAX_IMAGES)
                    cgi.Com_Error("Pic >= MAX_IMAGES");

                const char *const pic = cgi.get_configString(CS_IMAGES + value);

                if (pic && *pic)
                {
                    cgi.Draw_GetPicSize (&w, &h, pic);
                    cgi.SCR_DrawPic (x, y, w * scale, h * scale, pic);
                }
            }

            continue;
        }

        if (!strcmp(token, "client"))
        {   // draw a deathmatch client block
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                x = (hud_vrect.x + hud_vrect.width/2 + (atoi(token) - hx)) * scale;
                x += 8 * scale;
            }
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                y = (hud_vrect.y + hud_vrect.height/2 + (atoi(token) - hy)) * scale;
                y += 7 * scale;
            }

            token = COM_Parse (&s);

            if (!skip_depth)
            {
                value = atoi(token);
                if (value >= MAX_CLIENTS || value < 0)
                    cgi.Com_Error("client >= MAX_CLIENTS");
            }

            int score, ping;

            token = COM_Parse (&s);
            if (!skip_depth)
                score = atoi(token);

            token = COM_Parse (&s);
            if (!skip_depth)
            {
                ping = atoi(token);

                if (!scr_usekfont->integer)
                    CG_DrawString (x + 32 * scale, y, scale, cgi.CL_GetClientName(value));
                else
                    cgi.SCR_DrawFontString(cgi.CL_GetClientName(value), x + 32 * scale, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
                
                if (!scr_usekfont->integer)
                    CG_DrawString (x + 32 * scale, y + 10 * scale, scale, G_Fmt("{}", score).data(), true);
                else
                    cgi.SCR_DrawFontString(G_Fmt("{}", score).data(), x + 32 * scale, y + (10 - font_y_offset) * scale, scale, rgba_white, true, text_align_t::LEFT);

                cgi.SCR_DrawPic(x + 96 * scale, y + 10 * scale, 9 * scale, 9 * scale, "ping");
                
                if (!scr_usekfont->integer)
                    CG_DrawString (x + 73 * scale + 32 * scale, y + 10 * scale, scale, G_Fmt("{}", ping).data());
                else
                    cgi.SCR_DrawFontString (G_Fmt("{}", ping).data(), x + 107 * scale, y + (10 - font_y_offset) * scale, scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "ctf"))
        {   // draw a ctf client block
            int     score, ping;

            token = COM_Parse (&s);
            if (!skip_depth)
                x = (hud_vrect.x + hud_vrect.width/2 - hx + atoi(token)) * scale;
            token = COM_Parse (&s);
            if (!skip_depth)
                y = (hud_vrect.y + hud_vrect.height/2 - hy + atoi(token)) * scale;

            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = atoi(token);
                if (value >= MAX_CLIENTS || value < 0)
                    cgi.Com_Error("client >= MAX_CLIENTS");
            }

            token = COM_Parse (&s);
            if (!skip_depth)
                score = atoi(token);

            token = COM_Parse (&s);
            if (!skip_depth)
            {
                ping = atoi(token);
                if (ping > 999)
                    ping = 999;
            }

            token = COM_Parse (&s);

            if (!skip_depth)
            {

                cgi.SCR_DrawFontString (G_Fmt("{}", score).data(), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);
                x += 3 * 9 * scale;
                cgi.SCR_DrawFontString (G_Fmt("{}", ping).data(), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);
                x += 3 * 9 * scale;
                cgi.SCR_DrawFontString (cgi.CL_GetClientName(value), x, y - (font_y_offset * scale), scale, value == playernum ? alt_color : rgba_white, true, text_align_t::LEFT);

                if (*token)
                {
                    cgi.Draw_GetPicSize(&w, &h, token);
                    cgi.SCR_DrawPic(x - ((w + 2) * scale), y, w * scale, h * scale, token);
                }
            }
            continue;
        }

        if (!strcmp(token, "picn"))
        {   // draw a pic from a name
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                cgi.Draw_GetPicSize(&w, &h, token);
                cgi.SCR_DrawPic(x, y, w * scale, h * scale, token);
            }
            continue;
        }

        if (!strcmp(token, "num"))
        {   // draw a number
            token = COM_Parse (&s);
            if (!skip_depth)
                width = atoi(token);
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = ps->stats[atoi(token)];
                CG_DrawField (x, y, 0, width, value, scale);
            }
            continue;
        }
        // [Paril-KEX] special handling for the lives number
        else if (!strcmp(token, "lives_num"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                value = ps->stats[atoi(token)];
                CG_DrawField(x, y, value <= 2 ? flash_frame : 0, 1, max(0, value - 2), scale);
            }
        }

        if (!strcmp(token, "hnum"))
        {
            // health number
            if (!skip_depth)
            {
                int     color;

                width = 3;
                value = ps->stats[STAT_HEALTH];
                if (value > 25)
                    color = 0;  // green
                else if (value > 0)
                    color = flash_frame;      // flash
                else
                    color = 1;
                if (ps->stats[STAT_FLASHES] & 1)
                {
                    cgi.Draw_GetPicSize(&w, &h, "field_3");
                    cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
                }

                CG_DrawField (x, y, color, width, value, scale);
            }
            continue;
        }

        if (!strcmp(token, "anum"))
        {
            // ammo number
            if (!skip_depth)
            {
                int     color;

                width = 3;
                value = ps->stats[STAT_AMMO];

                int32_t min_ammo = CG_Wheel_GetWarnAmmoCount(ps->stats[STAT_ACTIVE_WEAPON]);

                if (!min_ammo)
                    min_ammo = 5; // back compat

                if (value > min_ammo)
                    color = 0;  // green
                else if (value >= 0)
                    color = flash_frame;      // flash
                else
                    continue;   // negative number = don't show
                if (ps->stats[STAT_FLASHES] & 4)
                {
                    cgi.Draw_GetPicSize(&w, &h, "field_3");
                    cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
                }

                CG_DrawField (x, y, color, width, value, scale);
            }
            continue;
        }

        if (!strcmp(token, "rnum"))
        {
            // armor number
            if (!skip_depth)
            {
                int     color;

                width = 3;
                value = ps->stats[STAT_ARMOR];
                if (value < 0)
                    continue;

                color = 0;  // green
                if (ps->stats[STAT_FLASHES] & 2)
                {
                    cgi.Draw_GetPicSize(&w, &h, "field_3");
                    cgi.SCR_DrawPic(x, y, w * scale, h * scale, "field_3");
                }

                CG_DrawField (x, y, color, width, value, scale);
            }
            continue;
        }

        if (!strcmp(token, "stat_string"))
        {
            token = COM_Parse (&s);

            if (!skip_depth)
            {
                index = atoi(token);
                if (index < 0 || index >= MAX_STATS)
                    cgi.Com_Error("Bad stat_string index");
                index = ps->stats[index];

                if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
                    index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

                if (index < 0 || index >= MAX_CONFIGSTRINGS)
                    cgi.Com_Error("Bad stat_string index");
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, cgi.get_configString(index));
                else
                    cgi.SCR_DrawFontString(cgi.get_configString(index), x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "cstring"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                CG_DrawHUDString (token, x, y, hx*2*scale, 0, scale);
            continue;
        }

        if (!strcmp(token, "string"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, token);
                else
                    cgi.SCR_DrawFontString(token, x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "cstring2"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
                CG_DrawHUDString (token, x, y, hx*2*scale, 0x80, scale);
            continue;
        }

        if (!strcmp(token, "string2"))
        {
            token = COM_Parse (&s);
            if (!skip_depth)
            {
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, token, true);
                else
                    cgi.SCR_DrawFontString(token, x, y - (font_y_offset * scale), scale, alt_color, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "if"))
        {
            // if stmt
            token = COM_Parse (&s);

            if_depth++;

            // skip to endif
            if (!skip_depth && !ps->stats[atoi(token)])
            {
                skip_depth = true;
                endif_depth = if_depth;
            }

            continue;
        }

        if (!strcmp(token, "ifgef"))
        {
            // if stmt
            token = COM_Parse (&s);

            if_depth++;

            // skip to endif
            if (!skip_depth && cgi.CL_ServerFrame() < atoi(token))
            {
                skip_depth = true;
                endif_depth = if_depth;
            }

            continue;
        }

        if (!strcmp(token, "endif"))
        {
            if (skip_depth && (if_depth == endif_depth))
                skip_depth = false;

            if_depth--;

            if (if_depth < 0)
                cgi.Com_Error("endif without matching if");

            continue;
        }

        // localization stuff
        if (!strcmp(token, "loc_stat_string"))
        {
            token = COM_Parse (&s);

            if (!skip_depth)
            {
                index = atoi(token);
                if (index < 0 || index >= MAX_STATS)
                    cgi.Com_Error("Bad stat_string index");
                index = ps->stats[index];

                if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
                    index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

                if (index < 0 || index >= MAX_CONFIGSTRINGS)
                    cgi.Com_Error("Bad stat_string index");
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, cgi.Localize(cgi.get_configString(index), nullptr, 0));
                else
                    cgi.SCR_DrawFontString(cgi.Localize(cgi.get_configString(index), nullptr, 0), x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "loc_stat_rstring"))
        {
            token = COM_Parse (&s);

            if (!skip_depth)
            {
                index = atoi(token);
                if (index < 0 || index >= MAX_STATS)
                    cgi.Com_Error("Bad stat_string index");
                index = ps->stats[index];

                if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
                    index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

                if (index < 0 || index >= MAX_CONFIGSTRINGS)
                    cgi.Com_Error("Bad stat_string index");
                const char *s = cgi.Localize(cgi.get_configString(index), nullptr, 0);
                if (!scr_usekfont->integer)
                    CG_DrawString (x - (static_cast<int>(CG_StrlenNoColor(s, strlen(s))) * CONCHAR_WIDTH * scale), y, scale, s);
                else
                {
                    vec2_t size = cgi.SCR_MeasureFontString(s, scale);
                    cgi.SCR_DrawFontString(s, x - size.x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
                }
            }
            continue;
        }
        
        if (!strcmp(token, "loc_stat_cstring"))
        {
            token = COM_Parse (&s);

            if (!skip_depth)
            {
                index = atoi(token);
                if (index < 0 || index >= MAX_STATS)
                    cgi.Com_Error("Bad stat_string index");
                index = ps->stats[index];

                if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
                    index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

                if (index < 0 || index >= MAX_CONFIGSTRINGS)
                    cgi.Com_Error("Bad stat_string index");
                CG_DrawHUDString (cgi.Localize(cgi.get_configString(index), nullptr, 0), x, y, hx*2*scale, 0, scale);
            }
            continue;
        }

        if (!strcmp(token, "loc_stat_cstring2"))
        {
            token = COM_Parse (&s);

            if (!skip_depth)
            {
                index = atoi(token);
                if (index < 0 || index >= MAX_STATS)
                    cgi.Com_Error("Bad stat_string index");
                index = ps->stats[index];

                if (cgi.CL_ServerProtocol() <= PROTOCOL_VERSION_3XX)
                    index = CS_REMAP(index).start / CS_MAX_STRING_LENGTH;

                if (index < 0 || index >= MAX_CONFIGSTRINGS)
                    cgi.Com_Error("Bad stat_string index");
                CG_DrawHUDString (cgi.Localize(cgi.get_configString(index), nullptr, 0), x, y, hx*2*scale, 0x80, scale);
            }
            continue;
        }

        static char arg_tokens[MAX_LOCALIZATION_ARGS + 1][MAX_TOKEN_CHARS];
        static const char *arg_buffers[MAX_LOCALIZATION_ARGS];

        if (!strcmp(token, "loc_cstring"))
        {
            int32_t num_args = atoi(COM_Parse (&s));

            if (num_args < 0 || num_args >= MAX_LOCALIZATION_ARGS)
                cgi.Com_Error("Bad loc string");

            // parse base
            token = COM_Parse (&s);
            Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

            // parse args
            for (int32_t i = 0; i < num_args; i++)
            {
                token = COM_Parse (&s);
                Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
                arg_buffers[i] = arg_tokens[1 + i];
            }

            if (!skip_depth)
                CG_DrawHUDString (cgi.Localize(arg_tokens[0], arg_buffers, num_args), x, y, hx*2*scale, 0, scale);
            continue;
        }

        if (!strcmp(token, "loc_string"))
        {
            int32_t num_args = atoi(COM_Parse (&s));

            if (num_args < 0 || num_args >= MAX_LOCALIZATION_ARGS)
                cgi.Com_Error("Bad loc string");

            // parse base
            token = COM_Parse (&s);
            Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

            // parse args
            for (int32_t i = 0; i < num_args; i++)
            {
                token = COM_Parse (&s);
                Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
                arg_buffers[i] = arg_tokens[1 + i];
            }
            
            if (!skip_depth)
            {
                if (!scr_usekfont->integer)
                    CG_DrawString (x, y, scale, cgi.Localize(arg_tokens[0], arg_buffers, num_args));
                else
                    cgi.SCR_DrawFontString(cgi.Localize(arg_tokens[0], arg_buffers, num_args), x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "loc_cstring2"))
        {
            int32_t num_args = atoi(COM_Parse (&s));

            if (num_args < 0 || num_args >= MAX_LOCALIZATION_ARGS)
                cgi.Com_Error("Bad loc string");

            // parse base
            token = COM_Parse (&s);
            Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

            // parse args
            for (int32_t i = 0; i < num_args; i++)
            {
                token = COM_Parse (&s);
                Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
                arg_buffers[i] = arg_tokens[1 + i];
            }
            
            if (!skip_depth)
                CG_DrawHUDString (cgi.Localize(arg_tokens[0], arg_buffers, num_args), x, y, hx*2*scale, 0x80, scale);
            continue;
        }

        if (!strcmp(token, "loc_string2") || !strcmp(token, "loc_rstring2") ||
            !strcmp(token, "loc_string") || !strcmp(token, "loc_rstring"))
        {
            bool green = token[strlen(token) - 1] == '2';
            bool rightAlign = !Q_strncasecmp(token, "loc_rstring", strlen("loc_rstring"));
            int32_t num_args = atoi(COM_Parse (&s));

            if (num_args < 0 || num_args >= MAX_LOCALIZATION_ARGS)
                cgi.Com_Error("Bad loc string");

            // parse base
            token = COM_Parse (&s);
            Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

            // parse args
            for (int32_t i = 0; i < num_args; i++)
            {
                token = COM_Parse (&s);
                Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
                arg_buffers[i] = arg_tokens[1 + i];
            }
            
            if (!skip_depth)
            {
                const char *locStr = cgi.Localize(arg_tokens[0], arg_buffers, num_args);
                int xOffs = 0;
                if (rightAlign)
                {
                    xOffs = scr_usekfont->integer ? cgi.SCR_MeasureFontString(locStr, scale).x : (static_cast<int>(CG_StrlenNoColor(locStr, strlen(locStr))) * CONCHAR_WIDTH * scale);
                }

                if (!scr_usekfont->integer)
                    CG_DrawString (x - xOffs, y, scale, locStr, green);
                else
                    cgi.SCR_DrawFontString(locStr, x - xOffs, y - (font_y_offset * scale), scale, green ? alt_color : rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        // draw time remaining
        if (!strcmp(token, "time_limit"))
        {
            // end frame
            token = COM_Parse (&s);

            if (!skip_depth)
            {
                int32_t end_frame = atoi(token);

                if (end_frame < cgi.CL_ServerFrame())
                    continue;

                uint64_t remaining_ms = (end_frame - cgi.CL_ServerFrame()) * cgi.frameTimeMs;

                const bool green = true;
                arg_buffers[0] = G_Fmt("{:02}:{:02}", (remaining_ms / 1000) / 60, (remaining_ms / 1000) % 60).data();

                const char *locStr = cgi.Localize("$g_score_time", arg_buffers, 1);
                int xOffs = scr_usekfont->integer ? cgi.SCR_MeasureFontString(locStr, scale).x : (static_cast<int>(CG_StrlenNoColor(locStr, strlen(locStr))) * CONCHAR_WIDTH * scale);
                if (!scr_usekfont->integer)
                    CG_DrawString (x - xOffs, y, scale, locStr, green);
                else
                    cgi.SCR_DrawFontString(locStr, x - xOffs, y - (font_y_offset * scale), scale, green ? alt_color : rgba_white, true, text_align_t::LEFT);
            }
        }

        // draw client dogtag
        if (!strcmp(token, "dogtag"))
        {
            token = COM_Parse (&s);
            
            if (!skip_depth)
            {
                value = atoi(token);
                if (value >= MAX_CLIENTS || value < 0)
                    cgi.Com_Error("client >= MAX_CLIENTS");

                const std::string_view path = G_Fmt("/tags/{}", cgi.CL_GetClientDogtag(value));
                cgi.SCR_DrawPic(x, y, 198 * scale, 32 * scale, path.data());
            }
        }

        if (!strcmp(token, "start_table"))
        {
            token = COM_Parse (&s);
            value = atoi(token);

            if (!skip_depth)
            {
                if (value >= q_countof(hud_temp.table_rows[0].table_cells))
                    cgi.Com_Error("table too big");

                hud_temp.num_columns = value;
                hud_temp.num_rows = 1;

                for (int i = 0; i < value; i++)
                    hud_temp.column_widths[i] = 0;
            }

            for (int i = 0; i < value; i++)
            {
                token = COM_Parse (&s);
                if (!skip_depth)
                {
                    token = cgi.Localize(token, nullptr, 0);
                    Q_strlcpy(hud_temp.table_rows[0].table_cells[i].text, token, sizeof(hud_temp.table_rows[0].table_cells[i].text));
                    hud_temp.column_widths[i] = max(hud_temp.column_widths[i], (size_t) cgi.SCR_MeasureFontString(hud_temp.table_rows[0].table_cells[i].text, scale).x);
                }
            }
        }

        if (!strcmp(token, "table_row"))
        {
            token = COM_Parse (&s);
            value = atoi(token);

            if (!skip_depth)
            {
                if (hud_temp.num_rows >= q_countof(hud_temp.table_rows))
                {
                    cgi.Com_Error("table too big");
                    return;
                }
            }
            
            auto &row = hud_temp.table_rows[hud_temp.num_rows];

            for (int i = 0; i < value; i++)
            {
                token = COM_Parse (&s);
                if (!skip_depth)
                {
                    Q_strlcpy(row.table_cells[i].text, token, sizeof(row.table_cells[i].text));
                    hud_temp.column_widths[i] = max(hud_temp.column_widths[i], (size_t) cgi.SCR_MeasureFontString(row.table_cells[i].text, scale).x);
                }
            }
            
            if (!skip_depth)
            {
                for (int i = value; i < hud_temp.num_columns; i++)
                    row.table_cells[i].text[0] = '\0';

                hud_temp.num_rows++;
            }
        }

        if (!strcmp(token, "draw_table"))
        {
            if (!skip_depth)
            {
                // in scaled pixels, incl padding between elements
                uint32_t total_inner_table_width = 0;

                for (int i = 0; i < hud_temp.num_columns; i++)
                {
                    if (i != 0)
                        total_inner_table_width += cgi.SCR_MeasureFontString(" ", scale).x;

                    total_inner_table_width += hud_temp.column_widths[i];
                }

                // in scaled pixels
                uint32_t total_table_height = hud_temp.num_rows * (CONCHAR_WIDTH + font_y_offset) * scale;

                CG_DrawTable(x, y, total_inner_table_width, total_table_height, scale);
            }
        }

        if (!strcmp(token, "stat_pname"))
        {
            token = COM_Parse(&s);

            if (!skip_depth)
            {
                index = atoi(token);
                if (index < 0 || index >= MAX_STATS)
                    cgi.Com_Error("Bad stat_string index");
                index = ps->stats[index] - 1;

                if (!scr_usekfont->integer)
                    CG_DrawString(x, y, scale, cgi.CL_GetClientName(index));
                else
                    cgi.SCR_DrawFontString(cgi.CL_GetClientName(index), x, y - (font_y_offset * scale), scale, rgba_white, true, text_align_t::LEFT);
            }
            continue;
        }

        if (!strcmp(token, "health_bars"))
        {
            if (skip_depth)
                continue;

            const byte *stat = reinterpret_cast<const byte *>(&ps->stats[STAT_HEALTH_BARS]);
            const char *name = cgi.Localize(CG_Hud_GetHealthBarName(), nullptr, 0);

            CG_DrawHUDString(name, (hud_vrect.x + hud_vrect.width/2 + -160) * scale, y, (320 / 2) * 2 * scale, 0, scale);

            float bar_width = ((hud_vrect.width * scale) - (hud_safe.x * 2)) * 0.50f;
            float bar_height = 4 * scale;

            y += cgi.SCR_FontLineHeight(scale);

            float x = ((hud_vrect.x + (hud_vrect.width * 0.5f)) * scale) - (bar_width * 0.5f);

            // 2 health bars, hardcoded
            for (size_t i = 0; i < 2; i++, stat++)
            {
                if (!(*stat & 0b10000000))
                    continue;

                float percent = (*stat & 0b01111111) / 127.f;

                cgi.SCR_DrawColorPic(x, y, bar_width + scale, bar_height + scale, "_white", rgba_black);

                if (percent > 0)
                    cgi.SCR_DrawColorPic(x, y, bar_width * percent, bar_height, "_white", rgba_red);
                if (percent < 1)
                    cgi.SCR_DrawColorPic(x + (bar_width * percent), y, bar_width * (1.f - percent), bar_height, "_white", { 80, 80, 80, 255 });

                y += bar_height * 3;
            }
        }

        if (!strcmp(token, "story"))
        {
            const char *story_str = CG_Hud_GetStory();

            if (!*story_str)
                continue;

            const char *localized = cgi.Localize(story_str, nullptr, 0);
            vec2_t size = cgi.SCR_MeasureFontString(localized, scale);
            float centerx = ((hud_vrect.x + (hud_vrect.width * 0.5f)) * scale);
            float centery = ((hud_vrect.y + (hud_vrect.height * 0.5f)) * scale) - (size.y * 0.5f);

            cgi.SCR_DrawFontString(localized, centerx, centery, scale, rgba_white, true, text_align_t::CENTER);
        }
    }

    if (skip_depth)
        cgi.Com_Error("if with no matching endif");
}

static cvar_t *cl_skipHud;
static cvar_t *cl_paused;
static cvar_t *cl_hud_cgame;

/*
================
CL_DrawInventory
================
*/
constexpr size_t DISPLAY_ITEMS   = 19;

static void CG_DrawInventory(const player_state_t *ps, const std::array<int16_t, MAX_ITEMS> &inventory, vrect_t hud_vrect, int32_t scale)
{
    int     i;
    int     num, selected_num, item;
    int     index[MAX_ITEMS];
    int     x, y;
    int     width, height;
    int     selected;
    int     top;

    selected = ps->stats[STAT_SELECTED_ITEM];

    num = 0;
    selected_num = 0;
    for (i=0 ; i<MAX_ITEMS ; i++) {
        if ( i == selected ) {
            selected_num = num;
        }
        if ( inventory[i] ) {
            index[num] = i;
            num++;
        }
    }

    // determine scroll point
    top = selected_num - DISPLAY_ITEMS/2;
    if (num - top < DISPLAY_ITEMS)
        top = num - DISPLAY_ITEMS;
    if (top < 0)
        top = 0;

    x = hud_vrect.x * scale;
    y = hud_vrect.y * scale;
    width = hud_vrect.width;
    height = hud_vrect.height;

    x += ((width / 2) - (256 / 2)) * scale;
    y += ((height / 2) - (216 / 2)) * scale;

    int pich, picw;
    cgi.Draw_GetPicSize(&picw, &pich, "inventory");
    cgi.SCR_DrawPic(x, y+8*scale, picw * scale, pich * scale, "inventory");

    y += 27 * scale;
    x += 22 * scale;

    for (i=top ; i<num && i < top+DISPLAY_ITEMS ; i++)
    {
        item = index[i];
        if (item == selected) // draw a blinky cursor by the selected item
        {
            if ( (cgi.CL_ClientRealTime() * 10) & 1)
                cgi.SCR_DrawChar(x-8, y, scale, 15, false);
        }

        if (!scr_usekfont->integer)
        {
            CG_DrawString(x, y, scale,
                G_Fmt("{:3} {}", inventory[item],
                    cgi.Localize(cgi.get_configString(CS_ITEMS + item), nullptr, 0)).data(),
                item == selected, false);
        }
        else
        {
            const char *string = G_Fmt("{}", inventory[item]).data();
            cgi.SCR_DrawFontString(string, x + (216 * scale) - (16 * scale), y - (font_y_offset * scale), scale, (item == selected) ? alt_color : rgba_white, true, text_align_t::RIGHT);

            string = cgi.Localize(cgi.get_configString(CS_ITEMS + item), nullptr, 0);
            cgi.SCR_DrawFontString(string, x + (16 * scale), y - (font_y_offset * scale), scale, (item == selected) ? alt_color : rgba_white, true, text_align_t::LEFT);
        }
            
        y += 8 * scale;
    }
}

struct cg_statusbar_builder_t {
    std::string sb;

    inline cg_statusbar_builder_t &yb(int32_t offset) { sb += G_Fmt("yb {} ", offset).data(); return *this; }
    inline cg_statusbar_builder_t &yt(int32_t offset) { sb += G_Fmt("yt {} ", offset).data(); return *this; }
    inline cg_statusbar_builder_t &yv(int32_t offset) { sb += G_Fmt("yv {} ", offset).data(); return *this; }
    inline cg_statusbar_builder_t &xl(int32_t offset) { sb += G_Fmt("xl {} ", offset).data(); return *this; }
    inline cg_statusbar_builder_t &xr(int32_t offset) { sb += G_Fmt("xr {} ", offset).data(); return *this; }
    inline cg_statusbar_builder_t &xv(int32_t offset) { sb += G_Fmt("xv {} ", offset).data(); return *this; }

    inline cg_statusbar_builder_t &ifstat(player_stat_t stat) { sb += G_Fmt("if {} ", static_cast<int>(stat)).data(); return *this; }
    inline cg_statusbar_builder_t &endifstat() { sb += "endif "; return *this; }

    inline cg_statusbar_builder_t &pic(player_stat_t stat) { sb += G_Fmt("pic {} ", static_cast<int>(stat)).data(); return *this; }
    inline cg_statusbar_builder_t &picn(const char *icon) { sb += G_Fmt("picn {} ", icon).data(); return *this; }

    inline cg_statusbar_builder_t &anum() { sb += "anum "; return *this; }
    inline cg_statusbar_builder_t &rnum() { sb += "rnum "; return *this; }
    inline cg_statusbar_builder_t &hnum() { sb += "hnum "; return *this; }
    inline cg_statusbar_builder_t &num(int32_t width, player_stat_t stat)
    {
        sb += G_Fmt("num {} {} ", width, static_cast<int>(stat)).data();
        return *this;
    }

    inline cg_statusbar_builder_t &loc_stat_string(player_stat_t stat)
    {
        sb += G_Fmt("loc_stat_string {} ", static_cast<int>(stat)).data();
        return *this;
    }
    inline cg_statusbar_builder_t &loc_stat_rstring(player_stat_t stat)
    {
        sb += G_Fmt("loc_stat_rstring {} ", static_cast<int>(stat)).data();
        return *this;
    }
    inline cg_statusbar_builder_t &stat_string(player_stat_t stat)
    {
        sb += G_Fmt("stat_string {} ", static_cast<int>(stat)).data();
        return *this;
    }
    inline cg_statusbar_builder_t &stat_string2(player_stat_t stat)
    {
        sb += G_Fmt("stat_string2 {} ", static_cast<int>(stat)).data();
        return *this;
    }
    inline cg_statusbar_builder_t &loc_stat_cstring2(player_stat_t stat)
    {
        sb += G_Fmt("loc_stat_cstring2 {} ", static_cast<int>(stat)).data();
        return *this;
    }
    inline cg_statusbar_builder_t &string2(const char *str)
    {
        if (str[0] != '"' && (strchr(str, ' ') || strchr(str, '\n')))
            sb += G_Fmt("string2 \"{}\" ", str).data();
        else
            sb += G_Fmt("string2 {} ", str).data();
        return *this;
    }
    inline cg_statusbar_builder_t &string(const char *str)
    {
        if (str[0] != '"' && (strchr(str, ' ') || strchr(str, '\n')))
            sb += G_Fmt("string \"{}\" ", str).data();
        else
            sb += G_Fmt("string {} ", str).data();
        return *this;
    }
    inline cg_statusbar_builder_t &loc_rstring(const char *str)
    {
        if (str[0] != '"' && (strchr(str, ' ') || strchr(str, '\n')))
            sb += G_Fmt("loc_rstring 0 \"{}\" ", str).data();
        else
            sb += G_Fmt("loc_rstring 0 {} ", str).data();
        return *this;
    }

    inline cg_statusbar_builder_t &lives_num(player_stat_t stat) { sb += G_Fmt("lives_num {} ", static_cast<int>(stat)).data(); return *this; }
    inline cg_statusbar_builder_t &stat_pname(player_stat_t stat) { sb += G_Fmt("stat_pname {} ", static_cast<int>(stat)).data(); return *this; }

    inline cg_statusbar_builder_t &health_bars() { sb += "health_bars "; return *this; }
    inline cg_statusbar_builder_t &story() { sb += "story "; return *this; }
};

static void CG_Statusbar_AddCombatHUD(cg_statusbar_builder_t &sb)
{
    sb.ifstat(STAT_SHOW_STATUSBAR)
        .xv(0).hnum().xv(50).pic(STAT_HEALTH_ICON)
        .ifstat(STAT_AMMO_ICON).xv(100).anum().xv(150).pic(STAT_AMMO_ICON).endifstat()
        .ifstat(STAT_ARMOR_ICON).xv(200).rnum().xv(250).pic(STAT_ARMOR_ICON).endifstat()
        .ifstat(STAT_SELECTED_ICON).xv(296).pic(STAT_SELECTED_ICON).endifstat()
        .endifstat();

    sb.yb(-50);

    sb.ifstat(STAT_SHOW_STATUSBAR)
        .ifstat(STAT_PICKUP_ICON).xv(0).pic(STAT_PICKUP_ICON).xv(26).yb(-42).loc_stat_string(STAT_PICKUP_STRING).yb(-50).endifstat()
        .ifstat(STAT_SELECTED_ITEM_NAME).yb(-34).xv(319).loc_stat_rstring(STAT_SELECTED_ITEM_NAME).yb(-58).endifstat()
        .endifstat();

    sb.ifstat(STAT_SHOW_STATUSBAR).ifstat(STAT_HELPICON).xv(150).pic(STAT_HELPICON).endifstat().endifstat();
}

static void CG_Statusbar_AddPowerupsAndTech(cg_statusbar_builder_t &sb)
{
    sb.ifstat(STAT_SHOW_STATUSBAR)
        .ifstat(STAT_POWERUP_ICON).xv(262).num(2, STAT_POWERUP_TIME).xv(296).pic(STAT_POWERUP_ICON).endifstat()
        .ifstat(STAT_TECH).yb(-137).xr(-26).pic(STAT_TECH).endifstat()
        .endifstat();
}

static void CG_Statusbar_AddCoopStatus(cg_statusbar_builder_t &sb, const player_state_t *ps)
{
    sb.ifstat(STAT_COOP_RESPAWN).xv(0).yt(0).loc_stat_cstring2(STAT_COOP_RESPAWN).endifstat();

    int y = 2;
    const int step = 26;

    if (ps->stats[STAT_LIVES] > 0) {
        sb.ifstat(STAT_LIVES).xr(-16).yt(y).lives_num(STAT_LIVES).xr(0).yt(y + step).loc_rstring("$g_lives").endifstat();
        y += step;
    }

    int rounds = ps->stats[STAT_ROUND_NUMBER];
    if (rounds > 0) {
        int chars = rounds > 99 ? 3 : rounds > 9 ? 2 : 1;
        y += 10;
        sb.ifstat(STAT_ROUND_NUMBER)
            .xr(-32 - (16 * chars)).yt(y).num(3, STAT_ROUND_NUMBER)
            .xr(0).yt(y + step).loc_rstring("Wave")
            .endifstat();
        y += step;
    }

    int monsters = ps->stats[STAT_MONSTER_COUNT];
    if (monsters > 0) {
        int chars = monsters > 99 ? 3 : monsters > 9 ? 2 : 1;
        y += 10;
        sb.ifstat(STAT_MONSTER_COUNT)
            .xr(-32 - (16 * chars)).yt(y).num(3, STAT_MONSTER_COUNT)
            .xr(0).yt(y + step).loc_rstring("Monsters")
            .endifstat();
        y += step;
    }
}

static void CG_Statusbar_AddSPExtras(cg_statusbar_builder_t &sb)
{
    sb.ifstat(STAT_POWERUP_ICON).yb(-76).endifstat();
    sb.ifstat(STAT_SELECTED_ITEM_NAME)
        .yb(-58)
        .ifstat(STAT_POWERUP_ICON).yb(-84).endifstat()
        .endifstat();

    sb.ifstat(STAT_KEY_A).xv(296).pic(STAT_KEY_A).endifstat();
    sb.ifstat(STAT_KEY_B).xv(272).pic(STAT_KEY_B).endifstat();
    sb.ifstat(STAT_KEY_C).xv(248).pic(STAT_KEY_C).endifstat();

    sb.ifstat(STAT_HEALTH_BARS).yt(24).health_bars().endifstat();

    sb.story();
}

static void CG_Statusbar_AddDeathmatchStatus(cg_statusbar_builder_t &sb, bool is_team_mode)
{
    if (is_team_mode) {
        sb.ifstat(STAT_CTF_FLAG_PIC).xr(-24).yt(26).pic(STAT_CTF_FLAG_PIC).endifstat();
        sb.ifstat(STAT_TEAMPLAY_INFO).xl(0).yb(-88).stat_string(STAT_TEAMPLAY_INFO).endifstat();
    }

    sb.ifstat(STAT_COUNTDOWN).xv(136).yb(-256).num(3, STAT_COUNTDOWN).endifstat();
    sb.ifstat(STAT_MATCH_STATE).xv(0).yb(-78).stat_string(STAT_MATCH_STATE).endifstat();

    sb.ifstat(STAT_FOLLOWING).xv(0).yb(-68).string2("FOLLOWING").xv(80).stat_string(STAT_FOLLOWING).endifstat();
    sb.ifstat(STAT_SPECTATOR).xv(0).yb(-68).string2("SPECTATING").xv(0).yb(-58).string("Use TAB Menu to join the match.").xv(80).endifstat();

    sb.ifstat(STAT_MINISCORE_FIRST_PIC).xr(-26).yb(-110).pic(STAT_MINISCORE_FIRST_PIC).xr(-78).num(3, STAT_MINISCORE_FIRST_SCORE).ifstat(STAT_MINISCORE_FIRST_VAL).xr(-24).yb(-94).stat_string(STAT_MINISCORE_FIRST_VAL).endifstat().endifstat();
    sb.ifstat(STAT_MINISCORE_FIRST_POS).xr(-28).yb(-112).pic(STAT_MINISCORE_FIRST_POS).endifstat();
    sb.ifstat(STAT_MINISCORE_SECOND_PIC).xr(-26).yb(-83).pic(STAT_MINISCORE_SECOND_PIC).xr(-78).num(3, STAT_MINISCORE_SECOND_SCORE).ifstat(STAT_MINISCORE_SECOND_VAL).xr(-24).yb(-68).stat_string(STAT_MINISCORE_SECOND_VAL).endifstat().endifstat();
    sb.ifstat(STAT_MINISCORE_SECOND_POS).xr(-28).yb(-85).pic(STAT_MINISCORE_SECOND_POS).endifstat();
    sb.ifstat(STAT_MINISCORE_FIRST_PIC).xr(-28).yb(-57).stat_string(STAT_SCORELIMIT).endifstat();

    sb.ifstat(STAT_CROSSHAIR_ID_VIEW).xv(122).yb(-128).stat_pname(STAT_CROSSHAIR_ID_VIEW).endifstat();
    sb.ifstat(STAT_CROSSHAIR_ID_VIEW_COLOR).xv(156).yb(-118).pic(STAT_CROSSHAIR_ID_VIEW_COLOR).endifstat();
}

static std::string CG_BuildStatusbarLayout(const player_state_t *ps)
{
    cg_statusbar_builder_t sb;
    sb.sb.reserve(1024);

    uint32_t hud_flags = CG_Hud_GetFlags();
    bool min_hud = (hud_flags & HUD_FLAG_MINHUD) != 0;

    sb.yb(-24);

    sb.ifstat(STAT_SHOW_STATUSBAR)
        .xv(min_hud ? 100 : 0).hnum().xv(min_hud ? 150 : 50).pic(STAT_HEALTH_ICON)
        .endifstat();

    if (!min_hud)
        CG_Statusbar_AddCombatHUD(sb);
    CG_Statusbar_AddPowerupsAndTech(sb);
    CG_Statusbar_AddCoopStatus(sb, ps);

    game_style_t style = CG_GetGameStyle();
    if (style == game_style_t::GAME_STYLE_PVE)
        CG_Statusbar_AddSPExtras(sb);
    else
        CG_Statusbar_AddDeathmatchStatus(sb, style == game_style_t::GAME_STYLE_TDM);

    return sb.sb;
}

static std::string CG_Scoreboard_EscapeText(const char *text)
{
    if (!text || !*text)
        return "";

    std::string out;
    out.reserve(strlen(text));
    for (const char *c = text; *c; ++c) {
        if (*c == '"' || *c == '\n' || *c == '\r' || *c == '\t')
            out.push_back(' ');
        else
            out.push_back(*c);
    }
    return out;
}

static const char *CG_Scoreboard_LimitLabel(uint32_t flags)
{
    if (flags & SB_FLAG_ROUNDS)
        return "Round Limit";
    if (flags & SB_FLAG_CTF)
        return "Capture Limit";
    return "Score Limit";
}

static std::string CG_Scoreboard_PlaceString(int rank, bool tied)
{
    static constexpr const char *suffix_table[10] = {
        "th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"
    };

    const int place = rank + 1;
    const int mod100 = place % 100;
    const char *suffix = "th";
    if (mod100 < 11 || mod100 > 13)
        suffix = suffix_table[place % 10];

    if (tied)
        return G_Fmt("Tied for {}{}", place, suffix).data();
    return G_Fmt("{}{}", place, suffix).data();
}

static std::string CG_BuildScoreboardLayout(const cg_scoreboard_data_t &sb, int playernum)
{
    std::string layout;
    layout.reserve(3072);

    std::string gametype = sb.gametype.empty() ? "Deathmatch" : sb.gametype;
    std::string map_name = CG_Scoreboard_EscapeText(CG_Hud_GetMapName());
    std::string gametype_name = CG_Scoreboard_EscapeText(gametype.c_str());
    std::string host_line = CG_Scoreboard_EscapeText(sb.host.c_str());
    std::string time_line = CG_Scoreboard_EscapeText(sb.match_time.c_str());
    std::string victor_line = CG_Scoreboard_EscapeText(sb.victor.c_str());
    if (map_name.empty())
        map_name = "unknown";

    layout += G_Fmt("xv 0 yv -40 cstring2 \"{} on '{}'\" ", gametype_name, map_name).data();
    layout += G_Fmt("xv 0 yv -30 cstring2 \"{}: {}\" ", CG_Scoreboard_LimitLabel(sb.flags), sb.score_limit).data();

    if (sb.flags & SB_FLAG_INTERMISSION) {
        if (!time_line.empty())
            layout += G_Fmt("xv 0 yv -50 cstring2 \"{}\" ", time_line).data();
        else if (!host_line.empty())
            layout += G_Fmt("xv 0 yv -50 cstring2 \"{}\" ", host_line).data();

        if (!victor_line.empty())
            layout += G_Fmt("xv 0 yv -10 cstring2 \"{}\" ", victor_line).data();

        if (sb.press_frame > 0) {
            layout += G_Fmt("ifgef {} yb -58 xv 0 cstring2 \"darkmatter-quake.com\" "
                            "yb -48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif ",
                            sb.press_frame).data();
        }
    } else if (!host_line.empty()) {
        layout += G_Fmt("xv 0 yv -50 cstring2 \"{}\" ", host_line).data();
    }

    if (sb.mode == SB_MODE_TEAM) {
        layout += G_Fmt("xv 0 yv -20 cstring2 \"Red {}  Blue {}\" ", sb.team_scores[0], sb.team_scores[1]).data();

        const size_t max_rows = 16;
        for (size_t i = 0; i < sb.team_rows[0].size() && i < max_rows; ++i) {
            const auto &row = sb.team_rows[0][i];
            const int y = 52 + static_cast<int>(i) * 8;
            const char *flag_icon = "\"\"";
            if (row.flags & SB_ROW_FLAG_BLUE)
                flag_icon = "sbfctf2";
            else if (row.flags & SB_ROW_FLAG_RED)
                flag_icon = "sbfctf1";

            if (row.flags & SB_ROW_READY)
                layout += G_Fmt("xv -56 yv {} picn wheel/p_compass_selected ", y - 2).data();

            layout += G_Fmt("ctf -40 {} {} {} {} {} ", y, row.client, row.score,
                            Q_clip(row.ping, 0, 999), flag_icon).data();
        }
        for (size_t i = 0; i < sb.team_rows[1].size() && i < max_rows; ++i) {
            const auto &row = sb.team_rows[1][i];
            const int y = 52 + static_cast<int>(i) * 8;
            const char *flag_icon = "\"\"";
            if (row.flags & SB_ROW_FLAG_BLUE)
                flag_icon = "sbfctf2";
            else if (row.flags & SB_ROW_FLAG_RED)
                flag_icon = "sbfctf1";

            if (row.flags & SB_ROW_READY)
                layout += G_Fmt("xv 182 yv {} picn wheel/p_compass_selected ", y - 2).data();

            layout += G_Fmt("ctf 200 {} {} {} {} {} ", y, row.client, row.score,
                            Q_clip(row.ping, 0, 999), flag_icon).data();
        }

        if (!sb.queued.empty() || !sb.spectators.empty()) {
            uint8_t last_red = 0;
            uint8_t last_blue = 0;
            if (!sb.team_rows[0].empty())
                last_red = static_cast<uint8_t>(min(sb.team_rows[0].size(), max_rows) - 1);
            if (!sb.team_rows[1].empty())
                last_blue = static_cast<uint8_t>(min(sb.team_rows[1].size(), max_rows) - 1);

            int y = ((max(last_red, last_blue) + 3) * 8) + 42;
            uint8_t lineIndex = 0;
            bool wroteQueued = false;

            for (const auto &spec : sb.queued) {
                if (!wroteQueued) {
                    layout += G_Fmt("xv 0 yv {} loc_string2 0 \"Queued Contenders:\" "
                                    "xv -40 yv {} loc_string2 0 \"w  l  name\" ",
                                    y, y + 8).data();
                    y += 16;
                    wroteQueued = true;
                }

                layout += G_Fmt("ctf {} {} {} {} {} \"\" ",
                                (lineIndex++ & 1) ? 200 : -40, y, spec.client,
                                spec.wins, spec.losses).data();
                if ((lineIndex & 1) == 0)
                    y += 8;
            }

            if (lineIndex & 1)
                y += 8;
            if (wroteQueued)
                y += 8;

            if (!sb.spectators.empty()) {
                layout += G_Fmt("xv 0 yv {} loc_string2 0 \"$g_pc_spectators\" ", y).data();
                y += 8;

                lineIndex = 0;
                for (const auto &spec : sb.spectators) {
                    layout += G_Fmt("ctf {} {} {} {} {} \"\" ",
                                    (lineIndex++ & 1) ? 200 : -40, y, spec.client,
                                    spec.score, Q_clip(spec.ping, 0, 999)).data();
                    if ((lineIndex & 1) == 0)
                        y += 8;
                }
            }
        }
    } else {
        const size_t max_rows = 16;
        for (size_t i = 0; i < sb.rows.size() && i < max_rows; ++i) {
            const auto &row = sb.rows[i];
            const int x = (i >= 8) ? 130 : -72;
            const int y = 32 * static_cast<int>(i % 8);
            if (row.flags & SB_ROW_READY)
                layout += G_Fmt("xv {} yv {} picn wheel/p_compass_selected ", x + 16, y + 16).data();
            layout += G_Fmt("client {} {} {} {} {} 0 ", x, y, row.client, row.score,
                            Q_clip(row.ping, 0, 999)).data();
        }

        if (sb.mode == SB_MODE_DUEL && (!sb.queued.empty() || !sb.spectators.empty())) {
            int y = 42;
            uint8_t lineIndex = 0;
            bool wroteQueued = false;

            for (const auto &spec : sb.queued) {
                if (!wroteQueued) {
                    layout += G_Fmt("xv 0 yv {} loc_string2 0 \"Queued Contenders:\" "
                                    "xv -40 yv {} loc_string2 0 \"w  l  name\" ",
                                    y, y + 8).data();
                    y += 16;
                    wroteQueued = true;
                }

                layout += G_Fmt("ctf {} {} {} {} {} \"\" ",
                                (lineIndex++ & 1) ? 200 : -40, y, spec.client,
                                spec.wins, spec.losses).data();
                if ((lineIndex & 1) == 0)
                    y += 8;
            }

            if (lineIndex & 1)
                y += 8;
            if (wroteQueued)
                y += 8;

            if (!sb.spectators.empty()) {
                layout += G_Fmt("xv 0 yv {} loc_string2 0 \"Spectators:\" ", y).data();
                y += 8;

                lineIndex = 0;
                for (const auto &spec : sb.spectators) {
                    layout += G_Fmt("ctf {} {} {} 0 0 \"\" ",
                                    (lineIndex++ & 1) ? 200 : -40, y, spec.client).data();
                    if ((lineIndex & 1) == 0)
                        y += 8;
                }
            }
        }
    }

    if (!(sb.flags & SB_FLAG_INTERMISSION) && sb.in_progress) {
        int playingCount = 0;
        int viewerScore = 0;
        int viewerTeam = -1;
        bool viewerFound = false;

        if (sb.mode == SB_MODE_TEAM) {
            playingCount = static_cast<int>(sb.team_rows[0].size() + sb.team_rows[1].size());
            for (size_t team = 0; team < sb.team_rows.size(); ++team) {
                for (const auto &row : sb.team_rows[team]) {
                    if (row.client == playernum) {
                        viewerScore = row.score;
                        viewerTeam = static_cast<int>(team);
                        viewerFound = true;
                        break;
                    }
                }
                if (viewerFound)
                    break;
            }
        } else {
            playingCount = static_cast<int>(sb.rows.size());
            for (const auto &row : sb.rows) {
                if (row.client == playernum) {
                    viewerScore = row.score;
                    viewerFound = true;
                    break;
                }
            }
        }

        if (viewerFound && viewerScore > 0 && playingCount > 1) {
            int rank = 0;
            bool tied = false;

            if (sb.mode == SB_MODE_TEAM) {
                if (sb.team_scores[0] == sb.team_scores[1]) {
                    rank = 2;
                } else if (viewerTeam == 0) {
                    rank = (sb.team_scores[0] > sb.team_scores[1]) ? 0 : 1;
                } else if (viewerTeam == 1) {
                    rank = (sb.team_scores[1] > sb.team_scores[0]) ? 0 : 1;
                }
            } else {
                for (const auto &row : sb.rows) {
                    if (row.client == playernum)
                        continue;
                    if (row.score > viewerScore)
                        rank++;
                    else if (row.score == viewerScore)
                        tied = true;
                }
            }

            const std::string place = CG_Scoreboard_PlaceString(rank, tied);
            layout += G_Fmt("xv 0 yv -10 cstring2 \"{} place with a score of {}\" ",
                            place, viewerScore).data();
        }

        const char *footer = (sb.mode == SB_MODE_TEAM)
            ? "Use inventory bind to toggle menu."
            : "Show inventory to toggle menu.";
        layout += G_Fmt("xv 0 yb -48 cstring2 \"{}\" ", footer).data();
    }

    return layout;
}

static std::string CG_FormatEOUTime(int time_ms)
{
    const int abs_ms = (time_ms >= 0) ? time_ms : -time_ms;
    const int minutes = abs_ms / 60000;
    const int seconds = (abs_ms / 1000) % 60;
    const int milliseconds = abs_ms % 1000;

    return G_Fmt("{:02}:{:02}:{:03}", minutes, seconds, milliseconds).data();
}

static std::string CG_BuildEOULayout(const cg_eou_data_t &eou)
{
    if (eou.rows.empty() && !eou.totals)
        return "";

    std::string layout;
    layout.reserve(2048);

    layout += "start_table 4 $m_eou_level $m_eou_kills $m_eou_secrets $m_eou_time ";

    int y = 16;
    for (const auto &row : eou.rows) {
        std::string name = CG_Scoreboard_EscapeText(row.name.c_str());
        if (name.empty())
            name = "???";

        layout += G_Fmt("yv {} table_row 4 \"{}\" {}/{} {}/{} {} ",
                        y, name, row.kills, row.total_kills,
                        row.secrets, row.total_secrets,
                        CG_FormatEOUTime(row.time_ms)).data();
        y += 8;
    }

    if (eou.totals) {
        y += 8;
        layout += "table_row 0 ";

        std::string name = CG_Scoreboard_EscapeText(eou.totals->name.c_str());
        if (name.empty())
            name = " ";

        layout += G_Fmt("yv {} table_row 4 \"{}\" {}/{} {}/{} {} ",
                        y, name, eou.totals->kills, eou.totals->total_kills,
                        eou.totals->secrets, eou.totals->total_secrets,
                        CG_FormatEOUTime(eou.totals->time_ms)).data();
    }

    layout += "xv 160 yt 0 draw_table ";

    if (eou.press_frame > 0) {
        layout += G_Fmt("ifgef {} yb -48 xv 0 loc_cstring2 0 \"$m_eou_press_button\" endif ",
                        eou.press_frame).data();
    }

    return layout;
}

static bool CG_DrawScoreboardFromBlob(vrect_t hud_vrect, vrect_t hud_safe, int32_t scale,
                                      int32_t playernum, const player_state_t *ps)
{
    if (!(ps->stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT))
        return false;
    if (CG_GetGameStyle() == game_style_t::GAME_STYLE_PVE)
        return false;

    CG_Hud_EnsureBlobParsed();
    if (!cg_scoreboard.valid)
        return false;

    if (cg_scoreboard.layout_dirty) {
        cg_scoreboard.layout_cache = CG_BuildScoreboardLayout(cg_scoreboard, playernum);
        cg_scoreboard.layout_dirty = false;
    }

    if (cg_scoreboard.layout_cache.empty())
        return false;

    CG_ExecuteLayoutString(cg_scoreboard.layout_cache.c_str(), hud_vrect, hud_safe,
                           scale, playernum, ps);
    return true;
}

static bool CG_DrawEOUFromBlob(vrect_t hud_vrect, vrect_t hud_safe, int32_t scale,
                               int32_t playernum, const player_state_t *ps)
{
    if (!(ps->stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT))
        return false;

    CG_Hud_EnsureBlobParsed();
    if (!cg_eou.valid)
        return false;

    if (cg_eou.layout_dirty) {
        cg_eou.layout_cache = CG_BuildEOULayout(cg_eou);
        cg_eou.layout_dirty = false;
    }

    if (cg_eou.layout_cache.empty())
        return false;

    CG_ExecuteLayoutString(cg_eou.layout_cache.c_str(), hud_vrect, hud_safe,
                           scale, playernum, ps);
    return true;
}

extern uint64_t cgame_init_time;

void CG_DrawHUD (int32_t isplit, const cg_server_data_t *data, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps)
{
    if (cgi.CL_InAutoDemoLoop())
    {
        if (cl_paused->integer) return; // demo is paused, menu is open

        uint64_t time = cgi.CL_ClientRealTime() - cgame_init_time;
        if (time < 20000 && 
            (time % 4000) < 2000)
            cgi.SCR_DrawFontString(cgi.Localize("$m_eou_press_button", nullptr, 0), hud_vrect.width * 0.5f * scale, (hud_vrect.height - 64.f) * scale, scale, rgba_green, true, text_align_t::CENTER);
        return;
    }

    // draw HUD
    if (!cl_skipHud->integer && !(ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD)) {
        if (cl_hud_cgame && cl_hud_cgame->integer) {
            const std::string layout = CG_BuildStatusbarLayout(ps);
            CG_ExecuteLayoutString(layout.c_str(), hud_vrect, hud_safe, scale, playernum, ps);
        } else {
            CG_ExecuteLayoutString(cgi.get_configString(CS_STATUSBAR), hud_vrect, hud_safe, scale, playernum, ps);
        }
    }

    // draw centerprint string
    CG_CheckDrawCenterString(ps, hud_vrect, hud_safe, isplit, scale);

    // draw notify
    CG_DrawNotify(isplit, hud_vrect, hud_safe, scale);

    // draw obituaries
    CG_DrawObituaries(isplit, hud_vrect, hud_safe, scale);

    // svc_layout still drawn with hud off
    bool drew_layout = false;
    if (cl_hud_cgame && cl_hud_cgame->integer) {
        drew_layout = CG_DrawEOUFromBlob(hud_vrect, hud_safe, scale,
                                         playernum, ps);
        if (!drew_layout) {
            drew_layout = CG_DrawScoreboardFromBlob(hud_vrect, hud_safe, scale,
                                                    playernum, ps);
        }
    }
    if ((ps->stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT) && !drew_layout)
        CG_ExecuteLayoutString(data->layout, hud_vrect, hud_safe, scale, playernum, ps);

    // inventory too
    if (ps->stats[STAT_LAYOUTS] & LAYOUTS_INVENTORY)
        CG_DrawInventory(ps, data->inventory, hud_vrect, scale);

    CG_WeaponBar_Draw(ps, hud_vrect, hud_safe, scale);
    CG_Wheel_Draw(ps, hud_vrect, hud_safe, scale);
}

/*
================
CG_TouchPics

================
*/
void CG_TouchPics()
{
    for (auto &nums : sb_nums)
        for (auto &str : nums)
            cgi.Draw_RegisterPic(str);

    cgi.Draw_RegisterPic("inventory");
    for (const auto &templ : obituary_templates) {
        if (templ.icon)
            cgi.Draw_RegisterPic(templ.icon);
    }
    CG_WeaponBar_Precache();
    CG_Wheel_Precache();

    font_y_offset = (cgi.SCR_FontLineHeight(1) - CONCHAR_WIDTH) / 2;
}

void CG_InitScreen()
{
    cl_paused = cgi.cvar("paused", "0", CVAR_NOFLAGS);
    cl_skipHud = cgi.cvar("cl_skipHud", "0", CVAR_ARCHIVE);
    cl_hud_cgame = cgi.cvar("cl_hud_cgame", "0", CVAR_ARCHIVE);
    scr_usekfont = cgi.cvar("scr_usekfont", "1", CVAR_NOFLAGS);
    scr_alpha = cgi.cvar("scr_alpha", "1", CVAR_NOFLAGS);
    scr_chathud = cgi.cvar("scr_chathud", "1", CVAR_NOFLAGS);

    scr_centertime  = cgi.cvar ("scr_centertime", "5.0",  CVAR_ARCHIVE); // [Sam-KEX] Changed from 2.5
    scr_printspeed  = cgi.cvar ("scr_printspeed", "0.04", CVAR_NOFLAGS); // [Sam-KEX] Changed from 8
    cl_notifytime   = cgi.cvar ("cl_notifytime", "5.0",   CVAR_ARCHIVE);
    scr_maxlines    = cgi.cvar ("scr_maxlines", "4",      CVAR_ARCHIVE);
    con_notifytime  = cgi.cvar ("con_notifytime", "0",    CVAR_NOFLAGS);
    con_notifylines = cgi.cvar ("con_notifylines", "4",   CVAR_NOFLAGS);
    cl_obituary_time = cgi.cvar("cl_obituary_time", "3000", CVAR_ARCHIVE);
    cl_obituary_fade = cgi.cvar("cl_obituary_fade", "200", CVAR_ARCHIVE);
    ui_acc_contrast = cgi.cvar ("ui_acc_contrast", "0",   CVAR_NOFLAGS);
    ui_acc_alttypeface = cgi.cvar("ui_acc_alttypeface", "0", CVAR_NOFLAGS);
    scr_crosshair = cgi.cvar("crosshair", "3", CVAR_ARCHIVE);
    cl_crosshair_brightness = cgi.cvar("cl_crosshairBrightness", "1.0", CVAR_ARCHIVE);
    cl_crosshair_color = cgi.cvar("cl_crosshairColor", "25", CVAR_ARCHIVE);
    cl_crosshair_health = cgi.cvar("cl_crosshairHealth", "0", CVAR_ARCHIVE);
    cl_crosshair_hit_color = cgi.cvar("cl_crosshairHitColor", "1", CVAR_ARCHIVE);
    cl_crosshair_hit_style = cgi.cvar("cl_crosshairHitStyle", "2", CVAR_ARCHIVE);
    cl_crosshair_hit_time = cgi.cvar("cl_crosshairHitTime", "200", CVAR_ARCHIVE);
    cl_crosshair_pulse = cgi.cvar("cl_crosshairPulse", "1", CVAR_ARCHIVE);
    cl_crosshair_size = cgi.cvar("cl_crosshairSize", "32", CVAR_ARCHIVE);
    cl_hit_markers = cgi.cvar("cl_hit_markers", "2", CVAR_NOFLAGS);
    ch_x = cgi.cvar("ch_x", "0", CVAR_NOFLAGS);
    ch_y = cgi.cvar("ch_y", "0", CVAR_NOFLAGS);
    scr_hit_marker_time = cgi.cvar("scr_hit_marker_time", "500", CVAR_NOFLAGS);
    scr_damage_indicators = cgi.cvar("scr_damage_indicators", "1", CVAR_NOFLAGS);
    scr_damage_indicator_time = cgi.cvar("scr_damage_indicator_time", "1000", CVAR_NOFLAGS);
    scr_pois = cgi.cvar("scr_pois", "1", CVAR_NOFLAGS);
    scr_poi_edge_frac = cgi.cvar("scr_poi_edge_frac", "0.15", CVAR_NOFLAGS);
    scr_poi_max_scale = cgi.cvar("scr_poi_max_scale", "1.0", CVAR_NOFLAGS);

    CG_Hud_Reset();

    hud_data = {};
    chat_hud = {};
    crosshair_state = {};
    poi_state = {};
}
