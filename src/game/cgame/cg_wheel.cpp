// Copyright (c) ZeniMax Media Inc.
// Licensed under the GNU General Public License 2.0.

#include "cg_wheel.h"

constexpr int32_t CONCHAR_WIDTH = 8;
constexpr int32_t CONCHAR_HEIGHT = 8;

static inline int CG_Rint(float x)
{
    return x < 0.0f ? static_cast<int>(x - 0.5f) : static_cast<int>(x + 0.5f);
}

static int CG_Snprintf(char *dest, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = std::vsnprintf(dest, size, fmt, args);
    va_end(args);

    if (ret < 0) {
        if (size)
            dest[0] = '\0';
        return 0;
    }

    return ret;
}

static int CG_Scnprintf(char *dest, size_t size, const char *fmt, ...)
{
    if (!size)
        return 0;

    va_list args;
    va_start(args, fmt);
    int ret = std::vsnprintf(dest, size, fmt, args);
    va_end(args);

    if (ret < 0) {
        dest[0] = '\0';
        return 0;
    }

    return min(ret, static_cast<int>(size - 1));
}

static inline void Vector2Clear(vec2_t &v)
{
    v.x = 0.0f;
    v.y = 0.0f;
}

static inline void Vector2Set(vec2_t &v, float x, float y)
{
    v.x = x;
    v.y = y;
}

static inline void Vector2Scale(const vec2_t &in, float scale, vec2_t &out)
{
    out.x = in.x * scale;
    out.y = in.y * scale;
}

static inline float Dot2Product(const vec2_t &a, const vec2_t &b)
{
    return (a.x * b.x) + (a.y * b.y);
}

static inline float Vector2Length(const vec2_t &v)
{
    return sqrtf(Dot2Product(v, v));
}

constexpr int32_t UI_LEFT = 1 << 0;
constexpr int32_t UI_RIGHT = 1 << 1;
constexpr int32_t UI_CENTER = UI_LEFT | UI_RIGHT;
constexpr int32_t UI_DROPSHADOW = 1 << 4;

constexpr int32_t STAT_MINUS = 10; // num frame for '-' stats digit
static constexpr const char *sb_nums[2][11] = {
    {
        "num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
        "num_6", "num_7", "num_8", "num_9", "num_minus"
    },
    {
        "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
        "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
    }
};

constexpr float WHEEL_CURSOR_RADIUS = 256.0f;
constexpr float WHEEL_DEADZONE_RADIUS = 144.0f;
constexpr float WHEEL_ITEM_RADIUS = 200.0f;
constexpr float WHEEL_REFERENCE_SIZE = 700.0f;
constexpr float WHEEL_REFERENCE_HEIGHT = 1440.0f;
constexpr float WHEEL_REFERENCE_CVAR = 175.0f; // 700px @ 1440p -> 175 in 16:9 virtual height
constexpr float WHEEL_ICON_BASE_SCALE = 1.5f;
constexpr float WHEEL_CENTER_COUNT_HEIGHT_FRAC = 0.1f;

constexpr int32_t WEAPON_BAR_DISABLED = 0;
constexpr int32_t WEAPON_BAR_STATIC_LEFT = 1;
constexpr int32_t WEAPON_BAR_STATIC_RIGHT = 2;
constexpr int32_t WEAPON_BAR_STATIC_CENTER = 3;
constexpr int32_t WEAPON_BAR_TIMED_Q3 = 4;
constexpr int32_t WEAPON_BAR_TIMED_Q2R = 5;

constexpr int32_t WEAPON_BAR_ICON_SIZE = 24 + 2;
constexpr int32_t WEAPON_BAR_PAD = 2;
constexpr float WEAPON_BAR_SIDE_CENTER_FRAC = 0.5f;
constexpr float WEAPON_BAR_CENTER_FRAC_Y = 0.79f;
constexpr float WEAPON_BAR_STATIC_SCALE = 0.5f;
constexpr int32_t WEAPON_BAR_NAME_OFFSET = 22;
constexpr float WEAPON_BAR_AMMO_ABOVE_FRAC = 0.625f;
constexpr int32_t WEAPON_BAR_TILE_INSET = 2;
constexpr int32_t MAX_WHEEL_VALUES = 8;

constexpr rgba_t WEAPON_BAR_TILE_BG { 96, 96, 96, 160 };

typedef struct {
    char main[MAX_QPATH];
    char wheel[MAX_QPATH];
    char selected[MAX_QPATH];
} cg_wheel_icon_t;

typedef struct {
    int item_index;
    cg_wheel_icon_t icons;
    int ammo_index;
    int min_ammo;
    int sort_id;
    int quantity_warn;
    bool is_powerup;
    bool can_drop;
} cg_wheel_weapon_t;

typedef struct {
    int item_index;
    cg_wheel_icon_t icons;
} cg_wheel_ammo_t;

typedef struct {
    int item_index;
    cg_wheel_icon_t icons;
    int sort_id;
    int ammo_index;
    bool is_toggle;
    bool can_drop;
} cg_wheel_powerup_t;

typedef enum {
    WHEEL_CLOSED,  // release holster
    WHEEL_CLOSING, // do not draw or process, but keep holster held
    WHEEL_OPEN     // draw & process + holster
} cg_wheel_state_t;

typedef struct {
    bool has_item;
    bool is_powerup;
    bool has_ammo;
    int data_id;
    int item_index;
    int sort_id;
    const cg_wheel_icon_t *icons;

    float popout;
    float icon_scale;
    float angle;
    vec2_t dir;
    float dot;
} cg_wheel_slot_t;

typedef struct {
    cg_wheel_weapon_t weapons[MAX_WHEEL_ITEMS];
    int num_weapons;

    cg_wheel_ammo_t ammo[MAX_WHEEL_ITEMS];
    int num_ammo;

    cg_wheel_powerup_t powerups[MAX_WHEEL_ITEMS];
    int num_powerups;
} cg_wheel_data_t;

typedef struct {
    cg_wheel_state_t state;
    uint64_t close_time;
    int selected;

    struct {
        bool has_ammo;
        int data_id;
        int item_index;
    } slots[MAX_WHEEL_ITEMS * 2];
    size_t num_slots;
} cg_weapon_bar_state_t;

typedef struct {
    cg_wheel_state_t state;
    vec2_t position;
    float distance;
    vec2_t dir;
    bool is_powerup_wheel;
    float timer;
    float timescale;

    cg_wheel_slot_t slots[MAX_WHEEL_ITEMS * 2];
    size_t num_slots;

    float slice_deg;
    float slice_sin;

    int selected;
    uint64_t deselect_time;
    uint64_t input_block_until;
} cg_wheel_state_info_t;

typedef struct {
    char wheel_circle[MAX_QPATH];
    int wheel_size;
    char wheel_button[MAX_QPATH];
    int wheel_button_size;
    char weapon_bar_selected[MAX_QPATH];
    int weapon_bar_selected_w;
    int weapon_bar_selected_h;
} cg_wheel_assets_t;

typedef struct {
    cg_wheel_assets_t assets;
    cg_wheel_data_t data;
    cg_weapon_bar_state_t weapon_bar;
    cg_wheel_state_info_t wheel;
    uint64_t last_wheel_time;
    uint64_t weapon_lock_time;
    vrect_t hud_vrect;
    vrect_t hud_safe;
    bool hud_valid;
    const player_state_t *last_ps;
} cg_wheel_state_global_t;

static cg_wheel_state_global_t cg_wheel;

static cvar_t *ww_ammo_size;
static cvar_t *ww_arrow_offset;
static cvar_t *ww_controller_exit_timeout;
static cvar_t *ww_deadzone_timeout;
static cvar_t *ww_hover_scale;
static cvar_t *ww_hover_time;
static cvar_t *ww_mouse_deadzone_speed;
static cvar_t *ww_mouse_sensitivity;
static cvar_t *ww_popout_amount;
static cvar_t *ww_popout_speed;
static cvar_t *ww_screen_frac_x;
static cvar_t *ww_screen_frac_y;
static cvar_t *ww_size;
static cvar_t *ww_timer_speed;
static cvar_t *ww_unavailable_shade_value;
static cvar_t *ww_underpic_nudge_amount;

static cvar_t *wb_screen_frac_y;
static cvar_t *wb_timeout;
static cvar_t *wb_lock_time;
static cvar_t *wb_ammo_scale;
static cvar_t *wb_mode;
static int weapon_bar_last_mode = WEAPON_BAR_TIMED_Q2R;

static void CG_WeaponBar_Cycle(int offset);
static uint64_t CG_Wheel_Now(void)
{
    if (cgi.CL_ClientRealTimeUnscaled)
        return cgi.CL_ClientRealTimeUnscaled();

    return cgi.CL_ClientRealTime();
}

static uint64_t CG_Wheel_ClientTime(void)
{
    if (cgi.CL_ClientTime)
        return cgi.CL_ClientTime();

    return CG_Wheel_Now();
}

static int CG_Wheel_HudWidth(void)
{
    if (cg_wheel.hud_valid && cg_wheel.hud_vrect.width > 0)
        return cg_wheel.hud_vrect.width;

    return 640;
}

static int CG_Wheel_HudHeight(void)
{
    if (cg_wheel.hud_valid && cg_wheel.hud_vrect.height > 0)
        return cg_wheel.hud_vrect.height;

    return 480;
}

static rgba_t CG_SetAlpha(rgba_t color, float alpha)
{
    float clamped = clamp(alpha, 0.0f, 1.0f);
    color.a = (uint8_t)clamp(color.a * clamped, 0.0f, 255.0f);
    return color;
}

static rgba_t CG_ShadeColor(int shade, float alpha)
{
    shade = clamp(shade, 0, 255);
    rgba_t color { (uint8_t)shade, (uint8_t)shade, (uint8_t)shade, 255 };
    return CG_SetAlpha(color, alpha);
}

static void CG_DrawFill(int x, int y, int w, int h, const rgba_t &color)
{
    if (w <= 0 || h <= 0)
        return;

    cgi.SCR_DrawColorPic(x, y, w, h, "_white", color);
}

static void CG_DrawScaledString(int x, int y, float scale, int flags, const rgba_t &color, const char *text)
{
    if (!text || !*text)
        return;

    if (scale <= 0.0f)
        scale = 1.0f;

    int char_w = max(1, CG_Rint(CONCHAR_WIDTH * scale));
    int char_h = max(1, CG_Rint(CONCHAR_HEIGHT * scale));
    int len = (int)strlen(text);

    if ((flags & UI_CENTER) == UI_CENTER) {
        x -= (len * char_w) / 2;
    } else if (flags & UI_RIGHT) {
        x -= len * char_w;
    }

    if (!cgi.SCR_DrawCharStretch) {
        int draw_scale = max(1, CG_Rint(scale));
        for (int i = 0; i < len; i++, x += CONCHAR_WIDTH * draw_scale) {
            cgi.SCR_DrawChar(x, y, draw_scale, text[i], (flags & UI_DROPSHADOW) != 0);
        }
        return;
    }

    for (int i = 0; i < len; i++, x += char_w) {
        cgi.SCR_DrawCharStretch(x, y, char_w, char_h, flags, text[i], color);
    }
}

static void CG_DrawScaledInt(int x, int y, float scale, int flags, const rgba_t &color, int value)
{
    char buffer[16];
    CG_Snprintf(buffer, sizeof(buffer), "%i", value);
    CG_DrawScaledString(x, y, scale, flags, color, buffer);
}

static void CG_DrawSbNumber(int x, int y, float height, float alpha, int value, bool alt)
{
    char buffer[16];
    int len = CG_Scnprintf(buffer, sizeof(buffer), "%i", value);
    if (len <= 0)
        return;

    const char *const *digits = sb_nums[alt ? 1 : 0];
    int digit_w = 0;
    int digit_h = 0;
    cgi.Draw_GetPicSize(&digit_w, &digit_h, digits[0]);
    if (digit_w <= 0 || digit_h <= 0)
        return;

    float scale = (height > 0.0f) ? (height / (float)digit_h) : 1.0f;
    int draw_w = max(1, CG_Rint(digit_w * scale));
    int draw_h = max(1, CG_Rint(digit_h * scale));
    int draw_x = x - (draw_w * len) / 2;
    int draw_y = y - (draw_h / 2);
    rgba_t color = CG_SetAlpha(rgba_white, alpha);

    for (int i = 0; i < len; i++) {
        char c = buffer[i];
        int frame;

        if (c == '-')
            frame = STAT_MINUS;
        else if (c >= '0' && c <= '9')
            frame = c - '0';
        else
            continue;

        cgi.SCR_DrawColorPic(draw_x, draw_y, draw_w, draw_h, digits[frame], color);
        draw_x += draw_w;
    }
}

static void CG_DrawPicShadowAlpha(int x, int y, int w, int h, const char *pic, int shadow_offset, float alpha)
{
    if (!pic || !*pic)
        return;

    rgba_t shadow = CG_SetAlpha(rgba_black, alpha);
    rgba_t main_color = CG_SetAlpha(rgba_white, alpha);
    cgi.SCR_DrawColorPic(x + shadow_offset, y + shadow_offset, w, h, pic, shadow);
    cgi.SCR_DrawColorPic(x, y, w, h, pic, main_color);
}

static void CG_DrawPicShadowColor(int x, int y, int w, int h, const char *pic, int shadow_offset, const rgba_t &color)
{
    if (!pic || !*pic)
        return;

    float alpha = (float)color.a / 255.0f;
    rgba_t shadow = CG_SetAlpha(rgba_black, alpha);
    cgi.SCR_DrawColorPic(x + shadow_offset, y + shadow_offset, w, h, pic, shadow);
    cgi.SCR_DrawColorPic(x, y, w, h, pic, color);
}

static bool CG_Wheel_PicExists(const char *name)
{
    if (!name || !*name)
        return false;

    int w = 0;
    int h = 0;
    cgi.Draw_GetPicSize(&w, &h, name);
    return w > 0 && h > 0;
}

static void CG_Wheel_LoadIcons(int icon_index, cg_wheel_icon_t *icons)
{
    if (!icons)
        return;

    memset(icons, 0, sizeof(*icons));

    const char *base = cgi.get_configString(CS_IMAGES + icon_index);
    if (!base || !*base)
        return;

    Q_strlcpy(icons->main, base, sizeof(icons->main));

    char path[MAX_QPATH];
    CG_Snprintf(path, sizeof(path), "wheel/%s", base);
    if (CG_Wheel_PicExists(path)) {
        Q_strlcpy(icons->wheel, path, sizeof(icons->wheel));
    } else {
        Q_strlcpy(icons->wheel, base, sizeof(icons->wheel));
    }

    CG_Snprintf(path, sizeof(path), "wheel/%s_selected", base);
    if (CG_Wheel_PicExists(path)) {
        Q_strlcpy(icons->selected, path, sizeof(icons->selected));
    } else {
        Q_strlcpy(icons->selected, icons->wheel, sizeof(icons->selected));
    }
}

static void CG_Wheel_ResetData(void)
{
    memset(&cg_wheel.data, 0, sizeof(cg_wheel.data));
}

static void CG_Wheel_RegisterIcon(const cg_wheel_icon_t *icons)
{
    if (!icons)
        return;

    if (icons->main[0])
        cgi.Draw_RegisterPic(icons->main);
    if (icons->wheel[0])
        cgi.Draw_RegisterPic(icons->wheel);
    if (icons->selected[0])
        cgi.Draw_RegisterPic(icons->selected);
}

static void CG_Wheel_RegisterAllIcons(void)
{
    for (int i = 0; i < cg_wheel.data.num_weapons; i++)
        CG_Wheel_RegisterIcon(&cg_wheel.data.weapons[i].icons);
    for (int i = 0; i < cg_wheel.data.num_ammo; i++)
        CG_Wheel_RegisterIcon(&cg_wheel.data.ammo[i].icons);
    for (int i = 0; i < cg_wheel.data.num_powerups; i++)
        CG_Wheel_RegisterIcon(&cg_wheel.data.powerups[i].icons);
}

void CG_Wheel_ParseConfigString(int32_t index, const char *s)
{
    if (!s || !*s)
        return;

    if (index < CS_WHEEL_WEAPONS || index >= (CS_WHEEL_POWERUPS + MAX_WHEEL_ITEMS))
        return;

    char entry[CS_MAX_STRING_LENGTH];
    Q_strlcpy(entry, s, sizeof(entry));

    int values[MAX_WHEEL_VALUES];
    size_t num_values = 0;

    for (char *start = entry; num_values < MAX_WHEEL_VALUES && start && *start; ) {
        char *end = strchr(start, '|');
        if (end)
            *end = '\0';

        char *endptr = nullptr;
        values[num_values++] = (int)strtol(start, &endptr, 10);
        if (endptr == start)
            return;

        start = end ? (end + 1) : nullptr;
    }

    if (index >= CS_WHEEL_POWERUPS) {
        if (num_values != 6)
            return;

        int slot = index - CS_WHEEL_POWERUPS;
        if (slot < 0 || slot >= MAX_WHEEL_ITEMS)
            return;

        cg_wheel.data.powerups[slot].item_index = values[0];
        CG_Wheel_LoadIcons(values[1], &cg_wheel.data.powerups[slot].icons);
        cg_wheel.data.powerups[slot].is_toggle = values[2] != 0;
        cg_wheel.data.powerups[slot].sort_id = values[3];
        cg_wheel.data.powerups[slot].can_drop = values[4] != 0;
        cg_wheel.data.powerups[slot].ammo_index = values[5];
        cg_wheel.data.num_powerups = max(slot + 1, cg_wheel.data.num_powerups);
        return;
    }

    if (index >= CS_WHEEL_AMMO) {
        if (num_values != 2)
            return;

        int slot = index - CS_WHEEL_AMMO;
        if (slot < 0 || slot >= MAX_WHEEL_ITEMS)
            return;

        cg_wheel.data.ammo[slot].item_index = values[0];
        CG_Wheel_LoadIcons(values[1], &cg_wheel.data.ammo[slot].icons);
        cg_wheel.data.num_ammo = max(slot + 1, cg_wheel.data.num_ammo);
        return;
    }

    if (num_values != 8)
        return;

    int slot = index - CS_WHEEL_WEAPONS;
    if (slot < 0 || slot >= MAX_WHEEL_ITEMS)
        return;

    cg_wheel.data.weapons[slot].item_index = values[0];
    CG_Wheel_LoadIcons(values[1], &cg_wheel.data.weapons[slot].icons);
    cg_wheel.data.weapons[slot].ammo_index = values[2];
    cg_wheel.data.weapons[slot].min_ammo = values[3];
    cg_wheel.data.weapons[slot].is_powerup = values[4] != 0;
    cg_wheel.data.weapons[slot].sort_id = values[5];
    cg_wheel.data.weapons[slot].quantity_warn = values[6];
    cg_wheel.data.weapons[slot].can_drop = values[7] != 0;
    cg_wheel.data.num_weapons = max(slot + 1, cg_wheel.data.num_weapons);
}

int32_t CG_Wheel_GetWarnAmmoCount(int32_t weapon_id)
{
    if (weapon_id < 0 || weapon_id >= cg_wheel.data.num_weapons)
        return 0;

    return cg_wheel.data.weapons[weapon_id].quantity_warn;
}

void CG_Wheel_Precache(void)
{
    Q_strlcpy(cg_wheel.assets.wheel_circle, "/gfx/weaponwheel.png", sizeof(cg_wheel.assets.wheel_circle));
    cgi.Draw_RegisterPic(cg_wheel.assets.wheel_circle);
    cgi.Draw_GetPicSize(&cg_wheel.assets.wheel_size, &cg_wheel.assets.wheel_size, cg_wheel.assets.wheel_circle);

    Q_strlcpy(cg_wheel.assets.wheel_button, "/gfx/wheelbutton.png", sizeof(cg_wheel.assets.wheel_button));
    cgi.Draw_RegisterPic(cg_wheel.assets.wheel_button);
    cgi.Draw_GetPicSize(&cg_wheel.assets.wheel_button_size, &cg_wheel.assets.wheel_button_size, cg_wheel.assets.wheel_button);

    CG_Wheel_RegisterAllIcons();

    cg_wheel.wheel.timescale = 1.0f;
}

void CG_Wheel_Init(void)
{
    ww_ammo_size = cgi.cvar("ww_ammo_size", "24.0", CVAR_SERVERINFO);
    ww_arrow_offset = cgi.cvar("ww_arrow_offset", "102.0", CVAR_SERVERINFO);
    ww_controller_exit_timeout = cgi.cvar("ww_controller_exit_timeout", "150", CVAR_USERINFO);
    ww_deadzone_timeout = cgi.cvar("ww_deadzone_timeout", "350", CVAR_USERINFO);
    ww_hover_scale = cgi.cvar("ww_hover_scale", "2.0", CVAR_SERVERINFO);
    ww_hover_time = cgi.cvar("ww_hover_time", "200", CVAR_SERVERINFO);
    ww_mouse_deadzone_speed = cgi.cvar("ww_mouse_deadzone_speed", "0.5", CVAR_SERVERINFO);
    ww_mouse_sensitivity = cgi.cvar("ww_mouse_sensitivity", "0.75", CVAR_SERVERINFO);
    ww_popout_amount = cgi.cvar("ww_popout_amount", "4.0", CVAR_SERVERINFO);
    ww_popout_speed = cgi.cvar("ww_popout_speed", "7.2", CVAR_SERVERINFO);
    ww_screen_frac_x = cgi.cvar("ww_screen_frac_x", "0.76", CVAR_SERVERINFO);
    ww_screen_frac_y = cgi.cvar("ww_screen_frac_y", "0.5", CVAR_SERVERINFO);
    ww_size = cgi.cvar("ww_size", "95.0", CVAR_SERVERINFO);
    ww_timer_speed = cgi.cvar("ww_timer_speed", "3.0", CVAR_SERVERINFO);
    ww_unavailable_shade_value = cgi.cvar("ww_unavailable_shade_value", "80", CVAR_USERINFO);
    ww_underpic_nudge_amount = cgi.cvar("ww_underpic_nudge_amount", "4.0", CVAR_SERVERINFO);

    cg_wheel = cg_wheel_state_global_t{};
    cg_wheel.wheel.state = WHEEL_CLOSED;
    cg_wheel.weapon_bar.state = WHEEL_CLOSED;
    cg_wheel.wheel.timescale = 1.0f;
    cg_wheel.weapon_bar.selected = -1;

    CG_Wheel_ResetData();
}

static bool CG_Wheel_GetItemDropName(int item_index, char *out_name, size_t out_size)
{
    if (!out_name || out_size == 0)
        return false;

    if (item_index < 0 || item_index >= MAX_ITEMS)
        return false;

    const char *raw = cgi.get_configString(CS_ITEMS + item_index);
    if (!raw || !*raw)
        return false;

    const char *localized = cgi.Localize(raw, nullptr, 0);
    if (!localized || !*localized)
        return false;

    Q_strlcpy(out_name, localized, out_size);
    return out_name[0] != '\0';
}

static uint32_t CG_Wheel_GetOwnedWeapons(const player_state_t *ps)
{
    if (!ps)
        return 0;

    return ((uint32_t)(uint16_t)ps->stats[STAT_WEAPONS_OWNED_1]) |
           ((uint32_t)(uint16_t)ps->stats[STAT_WEAPONS_OWNED_2] << 16);
}

static int16_t CG_Wheel_GetAmmoCount(const player_state_t *ps, int32_t ammo_id)
{
    if (!ps || ammo_id < 0)
        return 0;

    uint16_t ammo = GetAmmoStat((uint16_t *)&ps->stats[STAT_AMMO_INFO_START], ammo_id);
    if (ammo == AMMO_VALUE_INFINITE)
        return -1;

    return ammo;
}

static int16_t CG_Wheel_GetPowerupCount(const player_state_t *ps, int32_t powerup_id)
{
    if (!ps || powerup_id < 0)
        return 0;

    return GetPowerupStat((uint16_t *)&ps->stats[STAT_POWERUP_INFO_START], powerup_id);
}

static const cg_wheel_slot_t *CG_Wheel_GetSelectedSlot(void)
{
    if (cg_wheel.wheel.selected < 0 || cg_wheel.wheel.selected >= (int)cg_wheel.wheel.num_slots)
        return nullptr;

    const cg_wheel_slot_t *slot = &cg_wheel.wheel.slots[cg_wheel.wheel.selected];
    if (!slot->has_item)
        return nullptr;

    return slot;
}

static bool CG_Wheel_IsAmmoWeapon(const cg_wheel_weapon_t *weapon)
{
    if (!weapon || weapon->ammo_index < 0)
        return false;

    if (weapon->ammo_index >= cg_wheel.data.num_ammo)
        return false;

    return cg_wheel.data.ammo[weapon->ammo_index].item_index == weapon->item_index;
}

static bool CG_Wheel_CanDropAmmoForSlot(const player_state_t *ps, const cg_wheel_slot_t *slot)
{
    if (!ps || !slot || slot->is_powerup || !slot->has_item)
        return false;

    const cg_wheel_weapon_t *weapon = &cg_wheel.data.weapons[slot->data_id];
    if (weapon->ammo_index < 0 || weapon->ammo_index >= cg_wheel.data.num_ammo)
        return false;

    int count = CG_Wheel_GetAmmoCount(ps, weapon->ammo_index);
    if (count <= 0)
        return false;

    const cg_wheel_ammo_t *ammo = &cg_wheel.data.ammo[weapon->ammo_index];
    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CG_Wheel_GetItemDropName(ammo->item_index, drop_name, sizeof(drop_name)))
        return false;

    return true;
}

static bool CG_Wheel_CanDropWeaponForSlot(const player_state_t *ps, const cg_wheel_slot_t *slot)
{
    if (!ps || !slot || slot->is_powerup || !slot->has_item)
        return false;

    const cg_wheel_weapon_t *weapon = &cg_wheel.data.weapons[slot->data_id];
    if (!weapon->can_drop)
        return false;

    if (CG_Wheel_IsAmmoWeapon(weapon))
        return false;

    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CG_Wheel_GetItemDropName(slot->item_index, drop_name, sizeof(drop_name)))
        return false;

    return true;
}

static bool CG_Wheel_DropAmmo(const player_state_t *ps)
{
    if (cg_wheel.wheel.state != WHEEL_OPEN)
        return false;

    if (cg_wheel.wheel.is_powerup_wheel)
        return true;

    const cg_wheel_slot_t *slot = CG_Wheel_GetSelectedSlot();
    if (!slot || slot->is_powerup)
        return true;

    if (!CG_Wheel_CanDropAmmoForSlot(ps, slot))
        return true;

    const cg_wheel_weapon_t *weapon = &cg_wheel.data.weapons[slot->data_id];
    const cg_wheel_ammo_t *ammo = &cg_wheel.data.ammo[weapon->ammo_index];
    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CG_Wheel_GetItemDropName(ammo->item_index, drop_name, sizeof(drop_name)))
        return true;

    char cmd[CS_MAX_STRING_LENGTH + 16];
    CG_Snprintf(cmd, sizeof(cmd), "drop %s\n", drop_name);
    cgi.AddCommandString(cmd);
    return true;
}

static bool CG_Wheel_DropWeapon(const player_state_t *ps)
{
    if (cg_wheel.wheel.state != WHEEL_OPEN)
        return false;

    if (cg_wheel.wheel.is_powerup_wheel)
        return true;

    const cg_wheel_slot_t *slot = CG_Wheel_GetSelectedSlot();
    if (!slot || slot->is_powerup)
        return true;

    if (!CG_Wheel_CanDropWeaponForSlot(ps, slot))
        return true;

    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CG_Wheel_GetItemDropName(slot->item_index, drop_name, sizeof(drop_name)))
        return true;

    char cmd[CS_MAX_STRING_LENGTH + 16];
    CG_Snprintf(cmd, sizeof(cmd), "drop %s\n", drop_name);
    cgi.AddCommandString(cmd);
    return true;
}
void CG_Wheel_WeapNext(void)
{
    if (CG_Wheel_DropWeapon(cg_wheel.last_ps))
        return;

    CG_WeaponBar_Cycle(1);
}

void CG_Wheel_WeapPrev(void)
{
    if (CG_Wheel_DropAmmo(cg_wheel.last_ps))
        return;

    CG_WeaponBar_Cycle(-1);
}

static float CG_Wheel_GetDrawSize(void)
{
    float size_value = ww_size ? ww_size->value : WHEEL_REFERENCE_CVAR;
    size_value = clamp(size_value, 1.0f, 4096.0f);

    float base_size = CG_Wheel_HudHeight() * (WHEEL_REFERENCE_SIZE / WHEEL_REFERENCE_HEIGHT);
    return max(1.0f, base_size * (WHEEL_REFERENCE_CVAR / size_value));
}

static float CG_Wheel_GetScale(float draw_size)
{
    if (cg_wheel.assets.wheel_size <= 0)
        return 1.0f;

    return draw_size / (float)cg_wheel.assets.wheel_size;
}

static void CG_Wheel_GetCenter(float *out_x, float *out_y)
{
    float frac_x = ww_screen_frac_x ? ww_screen_frac_x->value : 0.76f;
    float frac_y = ww_screen_frac_y ? ww_screen_frac_y->value : 0.5f;
    frac_x = clamp(frac_x, 0.0f, 1.0f);
    frac_y = clamp(frac_y, 0.0f, 1.0f);

    float offset = frac_x - 0.5f;
    float width = (float)CG_Wheel_HudWidth();
    float height = (float)CG_Wheel_HudHeight();
    float base_x = cg_wheel.hud_valid ? (float)cg_wheel.hud_vrect.x : 0.0f;
    float base_y = cg_wheel.hud_valid ? (float)cg_wheel.hud_vrect.y : 0.0f;
    float center_x = base_x + (width * 0.5f) +
        ((cg_wheel.wheel.is_powerup_wheel ? -offset : offset) * width);
    float center_y = base_y + height * frac_y;

    if (out_x)
        *out_x = center_x;
    if (out_y)
        *out_y = center_y;
}

static void CG_Wheel_WarpCursor(float center_x, float center_y)
{
    if (!cgi.SCR_WarpMouse)
        return;

    cgi.SCR_WarpMouse(CG_Rint(center_x), CG_Rint(center_y));
}

static rgba_t CG_Wheel_SlotColor(bool available, float alpha)
{
    if (!available) {
        int shade = ww_unavailable_shade_value ? ww_unavailable_shade_value->integer : 80;
        return CG_ShadeColor(shade, alpha);
    }

    return CG_SetAlpha(rgba_white, alpha);
}

typedef struct {
    vec2_t pos;
    rgba_t color;
} wheel_poly_vert_t;

static void CG_DrawPolygon(const wheel_poly_vert_t *verts, int num)
{
    if (!verts || num != 3)
        return;

    float min_x = min(verts[0].pos.x, min(verts[1].pos.x, verts[2].pos.x));
    float max_x = max(verts[0].pos.x, max(verts[1].pos.x, verts[2].pos.x));
    float min_y = min(verts[0].pos.y, min(verts[1].pos.y, verts[2].pos.y));
    float max_y = max(verts[0].pos.y, max(verts[1].pos.y, verts[2].pos.y));

    int x0 = max(0, (int)floorf(min_x));
    int x1 = min(CG_Wheel_HudWidth() - 1, (int)ceilf(max_x));
    int y0 = max(0, (int)floorf(min_y));
    int y1 = min(CG_Wheel_HudHeight() - 1, (int)ceilf(max_y));

    float denom = (verts[1].pos.y - verts[2].pos.y) * (verts[0].pos.x - verts[2].pos.x) +
                  (verts[2].pos.x - verts[1].pos.x) * (verts[0].pos.y - verts[2].pos.y);
    if (fabsf(denom) < 0.0001f)
        return;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float sum_r = 0.0f;
            float sum_g = 0.0f;
            float sum_b = 0.0f;
            float sum_a = 0.0f;
            int hits = 0;
            const int sample_grid = 4;

            for (int sy = 0; sy < sample_grid; sy++) {
                for (int sx = 0; sx < sample_grid; sx++) {
                    float px = (float)x + ((float)sx + 0.5f) / (float)sample_grid;
                    float py = (float)y + ((float)sy + 0.5f) / (float)sample_grid;
                    float u = ((verts[1].pos.y - verts[2].pos.y) * (px - verts[2].pos.x) +
                               (verts[2].pos.x - verts[1].pos.x) * (py - verts[2].pos.y)) / denom;
                    float v = ((verts[2].pos.y - verts[0].pos.y) * (px - verts[2].pos.x) +
                               (verts[0].pos.x - verts[2].pos.x) * (py - verts[2].pos.y)) / denom;
                    float w = 1.0f - u - v;

                    if (denom < 0.0f) {
                        if (u > 0.0f || v > 0.0f || w > 0.0f)
                            continue;
                        u = -u;
                        v = -v;
                        w = -w;
                    } else {
                        if (u < 0.0f || v < 0.0f || w < 0.0f)
                            continue;
                    }

                    sum_r += verts[0].color.r * u + verts[1].color.r * v + verts[2].color.r * w;
                    sum_g += verts[0].color.g * u + verts[1].color.g * v + verts[2].color.g * w;
                    sum_b += verts[0].color.b * u + verts[1].color.b * v + verts[2].color.b * w;
                    sum_a += verts[0].color.a * u + verts[1].color.a * v + verts[2].color.a * w;
                    hits++;
                }
            }

            if (!hits)
                continue;

            float inv_hits = 1.0f / (float)hits;
            float coverage = (float)hits / (float)(sample_grid * sample_grid);
            float r = sum_r * inv_hits;
            float g = sum_g * inv_hits;
            float b = sum_b * inv_hits;
            float a = sum_a * inv_hits * coverage;

            rgba_t color {
                (uint8_t)clamp(r, 0.0f, 255.0f),
                (uint8_t)clamp(g, 0.0f, 255.0f),
                (uint8_t)clamp(b, 0.0f, 255.0f),
                (uint8_t)clamp(a, 0.0f, 255.0f)
            };
            CG_DrawFill(x, y, 1, 1, color);
        }
    }
}

static void CG_Wheel_DrawArrowTriangle(float center_x, float center_y, const vec2_t dir, float offset, float size, float alpha)
{
    float height = max(2.0f, size);
    float half_width = max(1.0f, height);

    vec2_t tip = { center_x + dir.x * offset, center_y + dir.y * offset };
    vec2_t base_center = { tip.x - dir.x * height, tip.y - dir.y * height };
    vec2_t perp = { -dir.y, dir.x };

    rgba_t tip_color = CG_SetAlpha(rgba_white, alpha);
    rgba_t base_color = CG_SetAlpha({ 0x43, 0x43, 0x43, 255 }, alpha);

    wheel_poly_vert_t verts[3];

    float soften = 1.12f;
    float soft_height = height * soften;
    float soft_half_width = half_width * soften;
    vec2_t soft_base = { tip.x - dir.x * soft_height, tip.y - dir.y * soft_height };
    rgba_t soft_tip = CG_SetAlpha(rgba_white, alpha * 0.35f);
    rgba_t soft_base_color = CG_SetAlpha({ 0x43, 0x43, 0x43, 255 }, alpha * 0.35f);

    verts[0].pos.x = tip.x;
    verts[0].pos.y = tip.y;
    verts[1].pos.x = soft_base.x + perp.x * soft_half_width;
    verts[1].pos.y = soft_base.y + perp.y * soft_half_width;
    verts[2].pos.x = soft_base.x - perp.x * soft_half_width;
    verts[2].pos.y = soft_base.y - perp.y * soft_half_width;
    verts[0].color = soft_tip;
    verts[1].color = soft_base_color;
    verts[2].color = soft_base_color;
    CG_DrawPolygon(verts, 3);

    verts[0].pos.x = tip.x;
    verts[0].pos.y = tip.y;
    verts[1].pos.x = base_center.x + perp.x * half_width;
    verts[1].pos.y = base_center.y + perp.y * half_width;
    verts[2].pos.x = base_center.x - perp.x * half_width;
    verts[2].pos.y = base_center.y - perp.y * half_width;
    verts[0].color = tip_color;
    verts[1].color = base_color;
    verts[2].color = base_color;
    CG_DrawPolygon(verts, 3);
}

static int CG_Wheel_SlotCompare(const void *a, const void *b)
{
    const cg_wheel_slot_t *sa = (const cg_wheel_slot_t *)a;
    const cg_wheel_slot_t *sb = (const cg_wheel_slot_t *)b;

    if (sa->sort_id == sb->sort_id)
        return sa->item_index - sb->item_index;

    return sa->sort_id - sb->sort_id;
}

static float CG_Wheel_GetPopoutForItem(const cg_wheel_slot_t *slots, size_t num_slots, int item_index)
{
    for (size_t i = 0; i < num_slots; i++) {
        if (slots[i].item_index == item_index)
            return slots[i].popout;
    }

    return 0.0f;
}

static float CG_Wheel_GetScaleForItem(const cg_wheel_slot_t *slots, size_t num_slots, int item_index)
{
    for (size_t i = 0; i < num_slots; i++) {
        if (slots[i].item_index == item_index)
            return slots[i].icon_scale;
    }

    return 1.0f;
}

static float CG_Wheel_GetHoverScale(void)
{
    if (!ww_hover_scale)
        return 2.0f;
    return max(0.0f, ww_hover_scale->value);
}

static float CG_Wheel_GetHoverSpeed(float selected_scale)
{
    float time_ms = ww_hover_time ? max(0.0f, ww_hover_time->value) : 200.0f;
    if (time_ms <= 0.0f)
        return 0.0f;

    float duration = time_ms * 0.001f;
    float delta = fabsf(selected_scale - 1.0f);
    if (delta <= 0.0f)
        return 0.0f;

    return delta / duration;
}

static void CG_Wheel_AdvanceValue(float *val, float target, float speed, float dt)
{
    if (!val || speed <= 0.0f || dt <= 0.0f)
        return;

    if (*val < target) {
        *val += speed * dt;
        if (*val > target)
            *val = target;
    } else if (*val > target) {
        *val -= speed * dt;
        if (*val < target)
            *val = target;
    }
}

static bool CG_Wheel_RebuildSlots(const player_state_t *ps, bool preserve_state)
{
    const player_state_t *state = ps ? ps : cg_wheel.last_ps;
    if (!state) {
        cg_wheel.wheel.num_slots = 0;
        cg_wheel.wheel.selected = -1;
        cg_wheel.wheel.deselect_time = 0;
        return false;
    }

    cg_wheel_slot_t old_slots[MAX_WHEEL_ITEMS * 2];
    size_t old_num_slots = 0;
    int selected_item = -1;

    if (preserve_state && cg_wheel.wheel.num_slots > 0) {
        old_num_slots = cg_wheel.wheel.num_slots;
        memcpy(old_slots, cg_wheel.wheel.slots, old_num_slots * sizeof(*old_slots));

        if (cg_wheel.wheel.selected >= 0 && cg_wheel.wheel.selected < (int)cg_wheel.wheel.num_slots)
            selected_item = cg_wheel.wheel.slots[cg_wheel.wheel.selected].item_index;
    }

    cg_wheel.wheel.num_slots = 0;

    if (cg_wheel.wheel.is_powerup_wheel) {
        const cg_wheel_powerup_t *powerup = cg_wheel.data.powerups;

        for (int i = 0; i < cg_wheel.data.num_powerups; i++, powerup++) {
            int count = CG_Wheel_GetPowerupCount(state, i);
            if (count <= 0)
                continue;

            cg_wheel_slot_t *slot = &cg_wheel.wheel.slots[cg_wheel.wheel.num_slots++];
            slot->data_id = i;
            slot->is_powerup = true;
            slot->item_index = powerup->item_index;
            slot->sort_id = powerup->sort_id;
            slot->icons = &powerup->icons;
            slot->has_item = true;
            slot->has_ammo = powerup->ammo_index == -1 ||
                             CG_Wheel_GetAmmoCount(state, powerup->ammo_index);
            slot->popout = preserve_state ? CG_Wheel_GetPopoutForItem(old_slots, old_num_slots, slot->item_index) : 0.0f;
            slot->icon_scale = preserve_state ? CG_Wheel_GetScaleForItem(old_slots, old_num_slots, slot->item_index) : 1.0f;
            if (slot->icon_scale <= 0.0f)
                slot->icon_scale = 1.0f;
        }
    } else {
        uint32_t owned = CG_Wheel_GetOwnedWeapons(state);
        const cg_wheel_weapon_t *weapon = cg_wheel.data.weapons;

        for (int i = 0; i < cg_wheel.data.num_weapons; i++, weapon++) {
            if (!(owned & (1u << i)))
                continue;

            cg_wheel_slot_t *slot = &cg_wheel.wheel.slots[cg_wheel.wheel.num_slots++];
            slot->data_id = i;
            slot->is_powerup = false;
            slot->item_index = weapon->item_index;
            slot->sort_id = weapon->sort_id;
            slot->icons = &weapon->icons;
            slot->has_item = true;
            slot->has_ammo = weapon->ammo_index == -1 ||
                             CG_Wheel_GetAmmoCount(state, weapon->ammo_index);
            slot->popout = preserve_state ? CG_Wheel_GetPopoutForItem(old_slots, old_num_slots, slot->item_index) : 0.0f;
            slot->icon_scale = preserve_state ? CG_Wheel_GetScaleForItem(old_slots, old_num_slots, slot->item_index) : 1.0f;
            if (slot->icon_scale <= 0.0f)
                slot->icon_scale = 1.0f;
        }
    }

    if (!cg_wheel.wheel.num_slots) {
        cg_wheel.wheel.selected = -1;
        cg_wheel.wheel.deselect_time = 0;
        return false;
    }

    qsort(cg_wheel.wheel.slots, cg_wheel.wheel.num_slots, sizeof(*cg_wheel.wheel.slots), CG_Wheel_SlotCompare);

    cg_wheel.wheel.slice_deg = ((PIf * 2.0f) / (float)cg_wheel.wheel.num_slots);
    cg_wheel.wheel.slice_sin = cosf(cg_wheel.wheel.slice_deg / 2.0f);

    cg_wheel.wheel.selected = -1;
    if (preserve_state && selected_item != -1) {
        for (int i = 0; i < (int)cg_wheel.wheel.num_slots; i++) {
            if (cg_wheel.wheel.slots[i].item_index == selected_item) {
                cg_wheel.wheel.selected = i;
                break;
            }
        }

        if (cg_wheel.wheel.selected == -1)
            cg_wheel.wheel.deselect_time = 0;
    }

    return true;
}
void CG_Wheel_Open(bool powerup)
{
    cg_wheel.wheel.is_powerup_wheel = powerup;
    cg_wheel.wheel.selected = -1;
    cg_wheel.wheel.deselect_time = 0;
    Vector2Clear(cg_wheel.wheel.position);
    cg_wheel.wheel.distance = 0.0f;
    Vector2Clear(cg_wheel.wheel.dir);

    if (!CG_Wheel_RebuildSlots(cg_wheel.last_ps, false))
        return;

    cg_wheel.wheel.state = WHEEL_OPEN;

    float center_x = 0.0f;
    float center_y = 0.0f;
    CG_Wheel_GetCenter(&center_x, &center_y);
    CG_Wheel_WarpCursor(center_x, center_y);
}

void CG_Wheel_ClearInput(void)
{
    if (cg_wheel.wheel.state == WHEEL_CLOSING)
        cg_wheel.wheel.state = WHEEL_CLOSED;
}

static void CG_Wheel_UseSelected(void)
{
    if (cg_wheel.wheel.selected < 0 || cg_wheel.wheel.selected >= (int)cg_wheel.wheel.num_slots)
        return;

    const cg_wheel_slot_t *slot = &cg_wheel.wheel.slots[cg_wheel.wheel.selected];
    if (!slot->has_item)
        return;

    char cmd[64];
    CG_Snprintf(cmd, sizeof(cmd), "use_index_only %i\n", slot->item_index);
    cgi.AddCommandString(cmd);
}

void CG_Wheel_Close(bool released)
{
    if (cg_wheel.wheel.state != WHEEL_OPEN)
        return;

    cg_wheel.wheel.state = WHEEL_CLOSING;
    if (ww_controller_exit_timeout) {
        int timeout = max(0, ww_controller_exit_timeout->integer);
        cg_wheel.wheel.input_block_until = CG_Wheel_Now() + timeout;
    }

    if (released)
        CG_Wheel_UseSelected();
}

void CG_Wheel_Input(int dx, int dy)
{
    if (cg_wheel.wheel.state == WHEEL_CLOSED)
        return;

    if (cg_wheel.wheel.state != WHEEL_OPEN)
        return;

    if (cg_wheel.wheel.input_block_until && CG_Wheel_Now() < cg_wheel.wheel.input_block_until)
        return;

    float wheel_draw_size = CG_Wheel_GetDrawSize();
    float wheel_scale = CG_Wheel_GetScale(wheel_draw_size);
    float deadzone_radius = WHEEL_DEADZONE_RADIUS * wheel_scale;
    float max_radius = WHEEL_CURSOR_RADIUS * wheel_scale;

    float deadzone_speed = ww_mouse_deadzone_speed ? ww_mouse_deadzone_speed->value : 0.5f;
    float move_speed = ww_mouse_sensitivity ? ww_mouse_sensitivity->value : 1.0f;
    float speed = (cg_wheel.wheel.distance <= deadzone_radius) ? deadzone_speed : move_speed;
    speed = clamp(speed, 0.0f, 10.0f);

    cg_wheel.wheel.position[0] += dx * speed;
    cg_wheel.wheel.position[1] += dy * speed;

    cg_wheel.wheel.distance = Vector2Length(cg_wheel.wheel.position);
    if (cg_wheel.wheel.distance > max_radius) {
        float scale = max_radius / cg_wheel.wheel.distance;
        Vector2Scale(cg_wheel.wheel.position, scale, cg_wheel.wheel.position);
        cg_wheel.wheel.distance = max_radius;
    }

    Vector2Clear(cg_wheel.wheel.dir);
    if (cg_wheel.wheel.distance > 0.0f) {
        float inv_distance = 1.0f / cg_wheel.wheel.distance;
        Vector2Scale(cg_wheel.wheel.position, inv_distance, cg_wheel.wheel.dir);
    }
}

void CG_Wheel_Update(const player_state_t *ps)
{
    cg_wheel.last_ps = ps;

    uint64_t t = CG_Wheel_Now();
    if (!cg_wheel.last_wheel_time)
        cg_wheel.last_wheel_time = t;
    float frac = (t - cg_wheel.last_wheel_time) * 0.001f;
    cg_wheel.last_wheel_time = t;
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 0.1f)
        frac = 0.1f;
    float timer_speed = ww_timer_speed ? ww_timer_speed->value : 0.0f;

    if (cg_wheel.wheel.state != WHEEL_OPEN) {
        if (cg_wheel.wheel.timer > 0.0f) {
            cg_wheel.wheel.timer = max(0.0f, cg_wheel.wheel.timer - (frac * timer_speed));
        }

        cg_wheel.wheel.timescale = max(0.1f, 1.0f - cg_wheel.wheel.timer);
        return;
    }

    CG_Wheel_RebuildSlots(ps, true);

    if (cg_wheel.wheel.timer < 1.0f) {
        cg_wheel.wheel.timer = min(1.0f, cg_wheel.wheel.timer + (frac * timer_speed));
    }

    cg_wheel.wheel.timescale = max(0.1f, 1.0f - cg_wheel.wheel.timer);

    if (!cg_wheel.wheel.num_slots)
        return;

    float wheel_scale = CG_Wheel_GetScale(CG_Wheel_GetDrawSize());
    float deadzone_radius = WHEEL_DEADZONE_RADIUS * wheel_scale;

    for (int i = 0; i < (int)cg_wheel.wheel.num_slots; i++) {
        cg_wheel.wheel.slots[i].angle = cg_wheel.wheel.slice_deg * i;
        Vector2Set(cg_wheel.wheel.slots[i].dir, sinf(cg_wheel.wheel.slots[i].angle), -cosf(cg_wheel.wheel.slots[i].angle));

        cg_wheel.wheel.slots[i].dot = Dot2Product(cg_wheel.wheel.dir, cg_wheel.wheel.slots[i].dir);
    }

    if (cg_wheel.wheel.selected >= 0 && cg_wheel.wheel.selected < (int)cg_wheel.wheel.num_slots) {
        if (!cg_wheel.wheel.slots[cg_wheel.wheel.selected].has_item) {
            cg_wheel.wheel.selected = -1;
            cg_wheel.wheel.deselect_time = 0;
        }
    }

    bool can_select = (cg_wheel.wheel.distance >= deadzone_radius);

    if (can_select) {
        int best = -1;
        float best_dot = cg_wheel.wheel.slice_sin;

        for (int i = 0; i < (int)cg_wheel.wheel.num_slots; i++) {
            if (!cg_wheel.wheel.slots[i].has_item)
                continue;

            if (cg_wheel.wheel.slots[i].dot > best_dot) {
                best = i;
                best_dot = cg_wheel.wheel.slots[i].dot;
            }
        }

        if (best != -1) {
            cg_wheel.wheel.selected = best;
            cg_wheel.wheel.deselect_time = 0;
        }
    } else if (cg_wheel.wheel.selected >= 0) {
        if (!cg_wheel.wheel.deselect_time)
            cg_wheel.wheel.deselect_time = CG_Wheel_Now() + max(0, ww_deadzone_timeout ? ww_deadzone_timeout->integer : 0);
    }

    if (cg_wheel.wheel.deselect_time && cg_wheel.wheel.deselect_time < CG_Wheel_Now()) {
        cg_wheel.wheel.selected = -1;
        cg_wheel.wheel.deselect_time = 0;
    }

    float popout_target = ww_popout_amount ? max(0.0f, ww_popout_amount->value) : 0.0f;
    float popout_speed = ww_popout_speed ? max(0.0f, ww_popout_speed->value) : 0.0f;
    float hover_scale = CG_Wheel_GetHoverScale();
    float selected_scale = clamp(hover_scale / WHEEL_ICON_BASE_SCALE, 0.1f, 4.0f);
    float hover_speed = CG_Wheel_GetHoverSpeed(selected_scale);
    for (int i = 0; i < (int)cg_wheel.wheel.num_slots; i++) {
        float target = (cg_wheel.wheel.selected == i && cg_wheel.wheel.slots[i].has_item) ? popout_target : 0.0f;
        CG_Wheel_AdvanceValue(&cg_wheel.wheel.slots[i].popout, target, popout_speed, frac);

        float scale_target = (cg_wheel.wheel.selected == i && cg_wheel.wheel.slots[i].has_item) ? selected_scale : 1.0f;
        if (hover_speed <= 0.0f)
            cg_wheel.wheel.slots[i].icon_scale = scale_target;
        else
            CG_Wheel_AdvanceValue(&cg_wheel.wheel.slots[i].icon_scale, scale_target, hover_speed, frac);
    }
}

void CG_Wheel_ApplyButtons(button_t *cmd_buttons)
{
    if (!cmd_buttons)
        return;

    if (cg_wheel.wheel.state != WHEEL_CLOSED && !cg_wheel.wheel.is_powerup_wheel)
        *cmd_buttons |= BUTTON_HOLSTER;
}

bool CG_Wheel_IsOpen(void)
{
    return cg_wheel.wheel.state == WHEEL_OPEN;
}

float CG_Wheel_TimeScale(void)
{
    return max(0.1f, cg_wheel.wheel.timescale);
}

bool CG_Wheel_AllowAttack(void)
{
    if (cg_wheel.wheel.state == WHEEL_OPEN)
        return false;

    return cg_wheel.weapon_lock_time <= CG_Wheel_ClientTime();
}

static rgba_t CG_Wheel_SlotCountColor(bool selected, bool warn_low)
{
    if (selected) {
        return warn_low ? rgba_t{ 255, 62, 33, 255 } : rgba_t{ 255, 248, 134, 255 };
    }

    return warn_low ? rgba_red : rgba_white;
}

static void CG_Wheel_DrawSlot(const player_state_t *ps, int slot_idx, float center_x, float center_y,
                              float wheel_scale, float wheel_alpha, int *out_slot_count, bool *out_warn_low)
{
    const cg_wheel_slot_t *slot = &cg_wheel.wheel.slots[slot_idx];
    bool selected = cg_wheel.wheel.selected == slot_idx;
    bool available = slot->has_item;

    vec2_t p;
    Vector2Scale(slot->dir, (WHEEL_ITEM_RADIUS + slot->popout) * wheel_scale, p);

    bool active = selected;
    float alpha = wheel_alpha;

    if (slot->is_powerup) {
        if (cg_wheel.data.powerups[slot->data_id].is_toggle) {
            if (CG_Wheel_GetPowerupCount(ps, slot->data_id) == 2)
                active = true;

            if (cg_wheel.data.powerups[slot->data_id].ammo_index != -1 &&
                !slot->has_ammo)
                alpha *= 0.5f;
        }
    }

    const char *icon = active ? slot->icons->selected : slot->icons->wheel;
    int icon_w = 0;
    int icon_h = 0;
    if (icon && *icon)
        cgi.Draw_GetPicSize(&icon_w, &icon_h, icon);
    if (icon_w <= 0)
        icon_w = 1;
    if (icon_h <= 0)
        icon_h = 1;

    float icon_scale = slot->icon_scale > 0.0f ? slot->icon_scale : 1.0f;
    float base_scale = WHEEL_ICON_BASE_SCALE;
    int draw_w = max(1, CG_Rint(icon_w * wheel_scale * base_scale * icon_scale));
    int draw_h = max(1, CG_Rint(icon_h * wheel_scale * base_scale * icon_scale));
    int draw_x = CG_Rint(center_x + p[0] - (draw_w / 2));
    int draw_y = CG_Rint(center_y + p[1] - (draw_h / 2));

    rgba_t icon_color = CG_Wheel_SlotColor(available, alpha);

    if (selected) {
        float nudge = ww_underpic_nudge_amount ? ww_underpic_nudge_amount->value : 0.0f;
        if (nudge != 0.0f) {
            vec2_t under_offset;
            Vector2Scale(slot->dir, -nudge * wheel_scale, under_offset);
            int under_x = CG_Rint(center_x + p[0] + under_offset[0] - (draw_w / 2));
            int under_y = CG_Rint(center_y + p[1] + under_offset[1] - (draw_h / 2));
            rgba_t under_color = CG_Wheel_SlotColor(available, alpha * 0.4f);
            CG_DrawPicShadowColor(under_x, under_y, draw_w, draw_h, slot->icons->wheel, 2, under_color);
        }
    }

    CG_DrawPicShadowColor(draw_x, draw_y, draw_w, draw_h, icon, 2, icon_color);

    int count = -1;
    bool warn_low = false;

    if (slot->is_powerup) {
        if (!cg_wheel.data.powerups[slot->data_id].is_toggle)
            count = CG_Wheel_GetPowerupCount(ps, slot->data_id);
        else if (cg_wheel.data.powerups[slot->data_id].ammo_index != -1)
            count = CG_Wheel_GetAmmoCount(ps, cg_wheel.data.powerups[slot->data_id].ammo_index);
    } else {
        if (cg_wheel.data.weapons[slot->data_id].ammo_index != -1) {
            count = CG_Wheel_GetAmmoCount(ps, cg_wheel.data.weapons[slot->data_id].ammo_index);
            warn_low = count <= cg_wheel.data.weapons[slot->data_id].quantity_warn;
        }
    }

    rgba_t count_color = CG_Wheel_SlotCountColor(selected, warn_low);

    int min_count = slot->is_powerup ? 2 : 0;
    if (count != -1 && count >= min_count) {
        float count_size = (ww_ammo_size ? ww_ammo_size->value : 24.0f) * wheel_scale;
        if (!slot->is_powerup)
            count_size *= 0.4f;
        float count_scale = count_size / (float)CONCHAR_HEIGHT;
        CG_DrawScaledInt(CG_Rint(center_x + p[0] + (draw_w / 2)),
                         CG_Rint(center_y + p[1] + (draw_h / 2)),
                         count_scale,
                         UI_CENTER | UI_DROPSHADOW,
                         CG_SetAlpha(count_color, wheel_alpha),
                         count);
    }

    if (out_slot_count)
        *out_slot_count = count;
    if (out_warn_low)
        *out_warn_low = warn_low;
}

static void CG_Wheel_DrawDropHints(const player_state_t *ps, float center_x, float center_y, float wheel_draw_size, float wheel_alpha)
{
    if (cg_wheel.wheel.is_powerup_wheel)
        return;

    const cg_wheel_slot_t *slot = CG_Wheel_GetSelectedSlot();
    if (!slot || slot->is_powerup)
        return;

    bool can_drop_weapon = CG_Wheel_CanDropWeaponForSlot(ps, slot);
    bool can_drop_ammo = CG_Wheel_CanDropAmmoForSlot(ps, slot);
    if (!can_drop_weapon && !can_drop_ammo)
        return;

    float scale = 0.75f;
    int line_height = max(1, CG_Rint(CONCHAR_HEIGHT * scale));
    int icon_size = max(1, line_height * 3);
    int padding = max(4, icon_size / 6);
    int line_gap = icon_size + 2;
    float wheel_scale = CG_Wheel_GetScale(wheel_draw_size);
    float deadzone_radius = WHEEL_DEADZONE_RADIUS * wheel_scale;
    int x = CG_Rint(center_x - deadzone_radius);
    int y = CG_Rint(center_y + (wheel_draw_size * 0.35f)) + (line_height / 2);
    rgba_t color = CG_SetAlpha(rgba_white, wheel_alpha);

    int line_index = 0;
    if (can_drop_weapon) {
        int line_y = y + (line_gap * line_index++);
        int icon_y = line_y - ((icon_size - line_height) / 2);
        const char *key_name = nullptr;
        int icon_w = 0;
        if (cgi.SCR_DrawBindIcon)
            icon_w = cgi.SCR_DrawBindIcon("cl_weapnext", x, icon_y, icon_size, color, &key_name);
        if (!key_name || !*key_name)
            key_name = cgi.CL_GetKeyBinding ? cgi.CL_GetKeyBinding("cl_weapnext") : "";
        int text_x = x + (icon_w > 0 ? icon_w + padding : 0);
        char line_weap[64];
        if (!key_name || !*key_name)
            CG_Snprintf(line_weap, sizeof(line_weap), "<UNBOUND> Drop Weapon");
        else if (icon_w > 0)
            CG_Snprintf(line_weap, sizeof(line_weap), "Drop Weapon");
        else
            CG_Snprintf(line_weap, sizeof(line_weap), "[%s] Drop Weapon", key_name);
        CG_DrawScaledString(text_x, line_y, scale, UI_DROPSHADOW, color, line_weap);
    }

    if (can_drop_ammo) {
        int line_y = y + (line_gap * line_index++);
        int icon_y = line_y - ((icon_size - line_height) / 2);
        const char *key_name = nullptr;
        int icon_w = 0;
        if (cgi.SCR_DrawBindIcon)
            icon_w = cgi.SCR_DrawBindIcon("cl_weapprev", x, icon_y, icon_size, color, &key_name);
        if (!key_name || !*key_name)
            key_name = cgi.CL_GetKeyBinding ? cgi.CL_GetKeyBinding("cl_weapprev") : "";
        int text_x = x + (icon_w > 0 ? icon_w + padding : 0);
        char line_ammo[64];
        if (!key_name || !*key_name)
            CG_Snprintf(line_ammo, sizeof(line_ammo), "<UNBOUND> Drop Ammo");
        else if (icon_w > 0)
            CG_Snprintf(line_ammo, sizeof(line_ammo), "Drop Ammo");
        else
            CG_Snprintf(line_ammo, sizeof(line_ammo), "[%s] Drop Ammo", key_name);
        CG_DrawScaledString(text_x, line_y, scale, UI_DROPSHADOW, color, line_ammo);
    }
}

void CG_Wheel_Draw(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int32_t scale)
{
    cg_wheel.hud_vrect = hud_vrect;
    cg_wheel.hud_safe = hud_safe;
    cg_wheel.hud_valid = true;

    const player_state_t *state = ps ? ps : cg_wheel.last_ps;
    if (!state)
        return;

    if (cg_wheel.wheel.state != WHEEL_OPEN && cg_wheel.wheel.timer == 0.0f)
        return;

    float center_x = 0.0f;
    float center_y = 0.0f;
    CG_Wheel_GetCenter(&center_x, &center_y);

    float t = 1.0f - cg_wheel.wheel.timer;
    float tween = 0.5f - (cosf((t * t) * PIf) * 0.5f);
    float wheel_alpha = 1.0f - tween;
    rgba_t base_color = CG_SetAlpha(rgba_white, wheel_alpha);
    rgba_t wheel_color = CG_SetAlpha(rgba_white, wheel_alpha * 0.8f);
    float wheel_draw_size = CG_Wheel_GetDrawSize();
    float wheel_scale = CG_Wheel_GetScale(wheel_draw_size);
    int wheel_draw_size_i = max(1, CG_Rint(wheel_draw_size));

    if (cg_wheel.assets.wheel_circle[0]) {
        cgi.SCR_DrawColorPic(CG_Rint(center_x - (wheel_draw_size_i / 2)),
                             CG_Rint(center_y - (wheel_draw_size_i / 2)),
                             wheel_draw_size_i,
                             wheel_draw_size_i,
                             cg_wheel.assets.wheel_circle,
                             wheel_color);
    }

    for (int i = 0; i < (int)cg_wheel.wheel.num_slots; i++) {
        if (i == cg_wheel.wheel.selected)
            continue;

        CG_Wheel_DrawSlot(state, i, center_x, center_y, wheel_scale, wheel_alpha, nullptr, nullptr);
    }

    if (cg_wheel.wheel.selected >= 0 && cg_wheel.wheel.selected < (int)cg_wheel.wheel.num_slots) {
        int count = -1;
        bool warn_low = false;
        CG_Wheel_DrawSlot(state, cg_wheel.wheel.selected, center_x, center_y, wheel_scale, wheel_alpha, &count, &warn_low);

        const cg_wheel_slot_t *slot = &cg_wheel.wheel.slots[cg_wheel.wheel.selected];
        if (slot->has_item) {
            const char *localized = cgi.Localize(cgi.get_configString(CS_ITEMS + slot->item_index), nullptr, 0);
            int name_y = CG_Rint(center_y - (wheel_draw_size * 0.125f) + (CONCHAR_HEIGHT * 3));
            if (!cg_wheel.wheel.is_powerup_wheel) {
                float text_scale = (scale > 0) ? (float)scale : 1.0f;
                int text_height = max(1, CG_Rint(CONCHAR_HEIGHT * text_scale));
                name_y -= text_height;
            }
            CG_DrawScaledString(CG_Rint(center_x), name_y, (float)scale, UI_CENTER | UI_DROPSHADOW, base_color, localized);

            float arrow_offset = (ww_arrow_offset ? ww_arrow_offset->value : 102.0f) * wheel_scale;
            float arrow_size = (ww_ammo_size ? ww_ammo_size->value : 24.0f) * wheel_scale;
            CG_Wheel_DrawArrowTriangle(center_x, center_y, slot->dir, arrow_offset, arrow_size, wheel_alpha);

            int ammo_index = -1;

            if (slot->is_powerup) {
                ammo_index = cg_wheel.data.powerups[slot->data_id].ammo_index;

                if (!cg_wheel.data.powerups[slot->data_id].is_toggle) {
                    float count_size = max(1.0f, CG_Wheel_HudHeight() * WHEEL_CENTER_COUNT_HEIGHT_FRAC);
                    CG_DrawSbNumber(CG_Rint(center_x),
                                    CG_Rint(center_y),
                                    count_size,
                                    wheel_alpha,
                                    CG_Wheel_GetPowerupCount(state, slot->data_id),
                                    false);
                }
            } else {
                ammo_index = cg_wheel.data.weapons[slot->data_id].ammo_index;
            }

            if (ammo_index != -1 && ammo_index < cg_wheel.data.num_ammo) {
                const cg_wheel_ammo_t *ammo = &cg_wheel.data.ammo[ammo_index];
                int ammo_w = 0;
                int ammo_h = 0;
                cgi.Draw_GetPicSize(&ammo_w, &ammo_h, ammo->icons.wheel);

                int icon_w = max(1, CG_Rint(ammo_w * wheel_scale * 3.0f));
                int icon_h = max(1, CG_Rint(ammo_h * wheel_scale * 3.0f));
                int icon_x = CG_Rint(center_x - (icon_w / 2));
                int icon_y = CG_Rint(center_y - (icon_h / 2));
                CG_DrawPicShadowAlpha(icon_x, icon_y, icon_w, icon_h, ammo->icons.wheel, 2, wheel_alpha);

                float count_size = max(1.0f, CG_Wheel_HudHeight() * WHEEL_CENTER_COUNT_HEIGHT_FRAC);
                int count_y = CG_Rint(center_y + (icon_h / 2) + (count_size * 0.5f));
                CG_DrawSbNumber(CG_Rint(center_x), count_y, count_size, wheel_alpha, count, false);
            }
        }
    }

    int wheel_button_draw_size = max(1, CG_Rint(cg_wheel.assets.wheel_button_size * wheel_scale));
    if (cg_wheel.assets.wheel_button[0]) {
        rgba_t button_color = CG_SetAlpha(rgba_white, wheel_alpha * 0.5f);
        cgi.SCR_DrawColorPic(CG_Rint(center_x + cg_wheel.wheel.position[0] - (wheel_button_draw_size / 2)),
                             CG_Rint(center_y + cg_wheel.wheel.position[1] - (wheel_button_draw_size / 2)),
                             wheel_button_draw_size,
                             wheel_button_draw_size,
                             cg_wheel.assets.wheel_button,
                             button_color);
    }

    CG_Wheel_DrawDropHints(state, center_x, center_y, wheel_draw_size, wheel_alpha);
}

static void CG_WeaponBar_GetSafeRect(int *out_x, int *out_y, int *out_w, int *out_h)
{
    int inset_x = max(0, cg_wheel.hud_safe.x);
    int inset_y = max(0, cg_wheel.hud_safe.y);

    int safe_w = max(0, CG_Wheel_HudWidth() - (inset_x * 2));
    int safe_h = max(0, CG_Wheel_HudHeight() - (inset_y * 2));

    if (out_x)
        *out_x = inset_x;
    if (out_y)
        *out_y = inset_y;
    if (out_w)
        *out_w = safe_w;
    if (out_h)
        *out_h = safe_h;
}

static int CG_WeaponBar_ClampStartInRange(int start, int size, int min_pos, int max_pos)
{
    int max_start = max(min_pos, max_pos - size);

    if (start < min_pos)
        return min_pos;
    if (start > max_start)
        return max_start;

    return start;
}

static void CG_WeaponBar_GetScaledCharSize(float scale, int *out_w, int *out_h)
{
    if (scale <= 0.0f)
        scale = 1.0f;

    if (out_w)
        *out_w = max(1, CG_Rint(CONCHAR_WIDTH * scale));
    if (out_h)
        *out_h = max(1, CG_Rint(CONCHAR_HEIGHT * scale));
}

static int CG_WeaponBar_GetScaledTextWidth(const char *text, float scale)
{
    if (!text || !*text)
        return 0;

    int char_w = 0;
    CG_WeaponBar_GetScaledCharSize(scale, &char_w, nullptr);
    return (int)strlen(text) * char_w;
}

static void CG_WeaponBar_DrawScaledString(int x, int y, float scale, int flags, const rgba_t &color, const char *text)
{
    CG_DrawScaledString(x, y, scale, flags, color, text);
}

static int CG_WeaponBar_FormatCount(int value, char *out, size_t out_size)
{
    int len = CG_Scnprintf(out, out_size, "%i", value);
    if (len < 0)
        return 0;

    return len;
}

static void CG_WeaponBar_GetScaledSize(const char *pic, float scale, int *out_w, int *out_h)
{
    int w = 0;
    int h = 0;

    if (pic && *pic)
        cgi.Draw_GetPicSize(&w, &h, pic);

    if (w <= 0)
        w = WEAPON_BAR_ICON_SIZE;
    if (h <= 0)
        h = WEAPON_BAR_ICON_SIZE;

    if (scale <= 0.0f)
        scale = 1.0f;

    if (out_w)
        *out_w = max(1, CG_Rint(w * scale));
    if (out_h)
        *out_h = max(1, CG_Rint(h * scale));
}

static void CG_WeaponBar_GetMaxIconSize(float scale, int *out_w, int *out_h)
{
    int max_w = 0;
    int max_h = 0;

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++) {
        const cg_wheel_weapon_t *weap = &cg_wheel.data.weapons[cg_wheel.weapon_bar.slots[i].data_id];
        const cg_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        CG_WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        max_w = max(max_w, icon_w);
        max_h = max(max_h, icon_h);
    }

    if (out_w)
        *out_w = max_w;
    if (out_h)
        *out_h = max_h;
}

static void CG_WeaponBar_DrawPicShadowScaled(int x, int y, const char *pic, float scale, int shadow_offset, int *out_w, int *out_h)
{
    int draw_w = 0;
    int draw_h = 0;
    int offset = max(1, CG_Rint(shadow_offset * scale));

    CG_WeaponBar_GetScaledSize(pic, scale, &draw_w, &draw_h);
    CG_DrawPicShadowColor(x, y, draw_w, draw_h, pic, offset, rgba_white);

    if (out_w)
        *out_w = draw_w;
    if (out_h)
        *out_h = draw_h;
}

static void CG_WeaponBar_DrawSelectionScaled(int icon_x, int icon_y, int icon_w, int icon_h, float scale)
{
    if (!cg_wheel.assets.weapon_bar_selected[0])
        return;

    int sel_w = 0;
    int sel_h = 0;
    CG_WeaponBar_GetScaledSize(cg_wheel.assets.weapon_bar_selected, scale, &sel_w, &sel_h);
    if (sel_w <= 0 || sel_h <= 0)
        return;

    int sel_x = icon_x + ((icon_w - sel_w) / 2);
    int sel_y = icon_y + ((icon_h - sel_h) / 2);
    cgi.SCR_DrawColorPic(sel_x, sel_y, sel_w, sel_h, cg_wheel.assets.weapon_bar_selected, rgba_white);
}

static int CG_WeaponBar_ClampMode(int mode)
{
    if (mode < WEAPON_BAR_DISABLED || mode > WEAPON_BAR_TIMED_Q2R)
        return WEAPON_BAR_TIMED_Q2R;

    return mode;
}

static int CG_WeaponBar_GetMode(void)
{
    if (!wb_mode)
        return WEAPON_BAR_TIMED_Q2R;

    return CG_WeaponBar_ClampMode(wb_mode->integer);
}

static void CG_WeaponBar_ApplyMode(int mode)
{
    if (mode == weapon_bar_last_mode)
        return;

    cg_wheel.weapon_bar.state = WHEEL_CLOSED;
    cg_wheel.weapon_bar.selected = -1;
    cg_wheel.weapon_bar.close_time = 0;
    weapon_bar_last_mode = mode;
}

static void CG_WeaponBar_Close(void)
{
    cg_wheel.weapon_bar.state = WHEEL_CLOSED;
}

static void CG_WeaponBar_SetSelectedFromActive(const player_state_t *ps)
{
    int active = ps ? ps->stats[STAT_ACTIVE_WEAPON] : -1;
    if (active >= 0 && active < cg_wheel.data.num_weapons)
        cg_wheel.weapon_bar.selected = cg_wheel.data.weapons[active].item_index;
    else
        cg_wheel.weapon_bar.selected = -1;
}

static bool CG_WeaponBar_Populate(const player_state_t *ps, bool prefer_active, bool strict_selection)
{
    if (!ps)
        return false;

    cg_wheel.weapon_bar.num_slots = 0;

    uint32_t owned = CG_Wheel_GetOwnedWeapons(ps);

    for (int i = 0; i < cg_wheel.data.num_weapons; i++) {
        if (!(owned & (1u << i)))
            continue;

        cg_wheel.weapon_bar.slots[cg_wheel.weapon_bar.num_slots].data_id = i;
        cg_wheel.weapon_bar.slots[cg_wheel.weapon_bar.num_slots].has_ammo =
            cg_wheel.data.weapons[i].ammo_index == -1 ||
            CG_Wheel_GetAmmoCount(ps, cg_wheel.data.weapons[i].ammo_index);
        cg_wheel.weapon_bar.slots[cg_wheel.weapon_bar.num_slots].item_index = cg_wheel.data.weapons[i].item_index;
        cg_wheel.weapon_bar.num_slots++;
    }

    if (!cg_wheel.weapon_bar.num_slots)
        return false;

    if (prefer_active)
        CG_WeaponBar_SetSelectedFromActive(ps);

    int i = 0;
    if (cg_wheel.weapon_bar.selected == -1) {
        cg_wheel.weapon_bar.selected = cg_wheel.weapon_bar.slots[0].item_index;
    } else {
        for (i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++)
            if (cg_wheel.weapon_bar.slots[i].item_index == cg_wheel.weapon_bar.selected)
                break;
    }

    if (i == (int)cg_wheel.weapon_bar.num_slots) {
        if (strict_selection)
            return false;

        cg_wheel.weapon_bar.selected = cg_wheel.weapon_bar.slots[0].item_index;
    }

    return true;
}

static void CG_WeaponBar_Open(const player_state_t *ps)
{
    if (cg_wheel.weapon_bar.state == WHEEL_CLOSED)
        CG_WeaponBar_SetSelectedFromActive(ps);

    cg_wheel.weapon_bar.state = WHEEL_OPEN;

    if (!CG_WeaponBar_Populate(ps, false, true)) {
        CG_WeaponBar_Close();
        return;
    }
}

static bool CG_WeaponBar_AdvanceSelection(int offset)
{
    if (cg_wheel.weapon_bar.num_slots < 2)
        return false;

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++) {
        if (cg_wheel.weapon_bar.slots[i].item_index != cg_wheel.weapon_bar.selected)
            continue;

        for (int n = 0, o = i + offset; n < (int)cg_wheel.weapon_bar.num_slots - 1; n++, o += offset) {
            if (o < 0)
                o = (int)cg_wheel.weapon_bar.num_slots - 1;
            else if (o >= (int)cg_wheel.weapon_bar.num_slots)
                o = 0;

            if (!cg_wheel.weapon_bar.slots[o].has_ammo)
                continue;

            cg_wheel.weapon_bar.selected = cg_wheel.weapon_bar.slots[o].item_index;
            return true;
        }

        break;
    }

    return false;
}

static rgba_t CG_WeaponBar_CountColor(const cg_wheel_weapon_t *weap, bool selected, int count)
{
    if (count <= weap->quantity_warn)
        return selected ? rgba_t{ 255, 83, 83, 255 } : rgba_red;

    return selected ? rgba_t{ 255, 255, 83, 255 } : rgba_white;
}

static void CG_WeaponBar_DrawHorizontalScaled(const player_state_t *ps, int bar_y, int selected_item, bool draw_ammo, bool ammo_above, float scale)
{
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;
    CG_WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int pad = max(1, CG_Rint(WEAPON_BAR_PAD * scale));
    int shadow_offset = max(1, CG_Rint(2 * scale));
    int bar_w = 0;
    int center_x = safe_x + (safe_w / 2);
    float ammo_scale = (wb_ammo_scale ? wb_ammo_scale->value : 0.66f) * scale;
    float ammo_effective_scale = max(1.0f, ammo_scale);
    int count_h = max(1, CG_Rint(CONCHAR_HEIGHT * ammo_effective_scale));
    int max_icon_h = 0;

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++) {
        const cg_wheel_weapon_t *weap = &cg_wheel.data.weapons[cg_wheel.weapon_bar.slots[i].data_id];
        const cg_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        CG_WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        bar_w += icon_w;
        max_icon_h = max(max_icon_h, icon_h);
        if (i < (int)cg_wheel.weapon_bar.num_slots - 1)
            bar_w += pad;
    }

    int bar_x = center_x - (bar_w / 2);
    bar_x = CG_WeaponBar_ClampStartInRange(bar_x, bar_w, safe_x, safe_x + safe_w);
    bar_y = CG_WeaponBar_ClampStartInRange(bar_y, max_icon_h, safe_y, safe_y + safe_h);

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++) {
        bool selected = cg_wheel.weapon_bar.slots[i].item_index == selected_item;
        const cg_wheel_weapon_t *weap = &cg_wheel.data.weapons[cg_wheel.weapon_bar.slots[i].data_id];
        const cg_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        CG_WeaponBar_DrawPicShadowScaled(bar_x, bar_y, selected ? icons->selected : icons->wheel, scale, shadow_offset, &icon_w, &icon_h);

        if (selected)
            CG_WeaponBar_DrawSelectionScaled(bar_x, bar_y, icon_w, icon_h, scale);

        if (draw_ammo && weap->ammo_index >= 0) {
            int count = CG_Wheel_GetAmmoCount(ps, weap->ammo_index);
            rgba_t color = CG_WeaponBar_CountColor(weap, selected, count);
            int ammo_pad = max(1, CG_Rint(2 * scale));
            int ammo_offset = max(max(1, CG_Rint(icon_h * WEAPON_BAR_AMMO_ABOVE_FRAC)), count_h + ammo_pad);
            int count_y = ammo_above ? (bar_y - ammo_offset) : (bar_y + icon_h + ammo_pad);

            CG_DrawScaledInt(bar_x + (icon_w / 2),
                             count_y,
                             ammo_scale,
                             UI_DROPSHADOW | UI_CENTER,
                             color,
                             count);
        }

        bar_x += icon_w + pad;
    }
}

typedef struct cg_weapon_bar_tile_metrics_s {
    int pad;
    int inset;
    int shadow_offset;
    int max_icon_w;
    int max_icon_h;
    int max_text_w;
    int char_w;
    int char_h;
    int tile_w;
    int tile_h;
} cg_weapon_bar_tile_metrics_t;

static void CG_WeaponBar_GetStaticTileMetrics(const player_state_t *ps, float scale, cg_weapon_bar_tile_metrics_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->pad = max(1, CG_Rint(WEAPON_BAR_PAD * scale));
    out->inset = max(1, CG_Rint(WEAPON_BAR_TILE_INSET * scale));
    out->shadow_offset = max(1, CG_Rint(2 * scale));

    CG_WeaponBar_GetScaledCharSize(scale, &out->char_w, &out->char_h);

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++) {
        const cg_wheel_weapon_t *weap = &cg_wheel.data.weapons[cg_wheel.weapon_bar.slots[i].data_id];
        const cg_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        CG_WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        out->max_icon_w = max(out->max_icon_w, icon_w);
        out->max_icon_h = max(out->max_icon_h, icon_h);

        if (weap->ammo_index >= 0) {
            int count = CG_Wheel_GetAmmoCount(ps, weap->ammo_index);
            char count_text[16];
            CG_WeaponBar_FormatCount(count, count_text, sizeof(count_text));
            out->max_text_w = max(out->max_text_w, CG_WeaponBar_GetScaledTextWidth(count_text, scale));
        }
    }

    out->tile_w = out->max_icon_w + out->max_text_w + (out->pad + (out->inset * 2));
    out->tile_h = max(out->max_icon_h, out->char_h) + (out->inset * 2);
}

static void CG_WeaponBar_DrawStaticTilesVertical(const player_state_t *ps, int selected_item, bool right_side)
{
    float scale = WEAPON_BAR_STATIC_SCALE;
    cg_weapon_bar_tile_metrics_t metrics;
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;

    CG_WeaponBar_GetStaticTileMetrics(ps, scale, &metrics);
    CG_WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int total_h = (int)cg_wheel.weapon_bar.num_slots * metrics.tile_h + ((int)cg_wheel.weapon_bar.num_slots - 1) * metrics.pad;
    int start_y = safe_y + CG_Rint((float)safe_h * WEAPON_BAR_SIDE_CENTER_FRAC) - (total_h / 2);
    start_y = CG_WeaponBar_ClampStartInRange(start_y, total_h, safe_y, safe_y + safe_h);

    int start_x = right_side ? (safe_x + safe_w - metrics.tile_w) : safe_x;
    start_x = CG_WeaponBar_ClampStartInRange(start_x, metrics.tile_w, safe_x, safe_x + safe_w);
    int text_flags = UI_DROPSHADOW | (right_side ? UI_RIGHT : 0);

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++) {
        bool selected = cg_wheel.weapon_bar.slots[i].item_index == selected_item;
        const cg_wheel_weapon_t *weap = &cg_wheel.data.weapons[cg_wheel.weapon_bar.slots[i].data_id];
        const cg_wheel_icon_t *icons = &weap->icons;
        int tile_x = start_x;
        int tile_y = start_y + (i * (metrics.tile_h + metrics.pad));

        if (selected)
            CG_DrawFill(tile_x, tile_y, metrics.tile_w, metrics.tile_h, WEAPON_BAR_TILE_BG);

        int icon_w = 0;
        int icon_h = 0;
        CG_WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        int icon_x = right_side ? (tile_x + metrics.tile_w - metrics.inset - icon_w) : (tile_x + metrics.inset);
        int icon_y = tile_y + metrics.inset + ((metrics.tile_h - (metrics.inset * 2) - icon_h) / 2);

        CG_WeaponBar_DrawPicShadowScaled(icon_x, icon_y, selected ? icons->selected : icons->wheel, scale, metrics.shadow_offset, &icon_w, &icon_h);

        if (selected)
            CG_WeaponBar_DrawSelectionScaled(icon_x, icon_y, icon_w, icon_h, scale);

        if (weap->ammo_index >= 0) {
            int count = CG_Wheel_GetAmmoCount(ps, weap->ammo_index);
            rgba_t color = CG_WeaponBar_CountColor(weap, selected, count);
            char count_text[16];
            CG_WeaponBar_FormatCount(count, count_text, sizeof(count_text));

            int text_x = right_side ? (icon_x - metrics.pad) : (tile_x + metrics.inset + metrics.max_icon_w + metrics.pad);
            int text_y = tile_y + ((metrics.tile_h - metrics.char_h) / 2);

            CG_WeaponBar_DrawScaledString(text_x, text_y, scale, text_flags, color, count_text);
        }
    }
}

static void CG_WeaponBar_DrawStaticTilesHorizontal(const player_state_t *ps, int selected_item)
{
    float scale = WEAPON_BAR_STATIC_SCALE;
    cg_weapon_bar_tile_metrics_t metrics;
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;

    CG_WeaponBar_GetStaticTileMetrics(ps, scale, &metrics);
    CG_WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int total_w = (int)cg_wheel.weapon_bar.num_slots * metrics.tile_w + ((int)cg_wheel.weapon_bar.num_slots - 1) * metrics.pad;
    int start_x = safe_x + (safe_w / 2) - (total_w / 2);
    int start_y = safe_y + CG_Rint(safe_h * WEAPON_BAR_CENTER_FRAC_Y) + (metrics.tile_h / 2);
    start_x = CG_WeaponBar_ClampStartInRange(start_x, total_w, safe_x, safe_x + safe_w);
    start_y = CG_WeaponBar_ClampStartInRange(start_y, metrics.tile_h, safe_y, safe_y + safe_h);

    int text_flags = UI_DROPSHADOW;

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++) {
        bool selected = cg_wheel.weapon_bar.slots[i].item_index == selected_item;
        const cg_wheel_weapon_t *weap = &cg_wheel.data.weapons[cg_wheel.weapon_bar.slots[i].data_id];
        const cg_wheel_icon_t *icons = &weap->icons;
        int tile_x = start_x + (i * (metrics.tile_w + metrics.pad));
        int tile_y = start_y;

        if (selected)
            CG_DrawFill(tile_x, tile_y, metrics.tile_w, metrics.tile_h, WEAPON_BAR_TILE_BG);

        int icon_w = 0;
        int icon_h = 0;
        int icon_x = tile_x + metrics.inset;
        CG_WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        int icon_y = tile_y + metrics.inset + ((metrics.tile_h - (metrics.inset * 2) - icon_h) / 2);

        CG_WeaponBar_DrawPicShadowScaled(icon_x, icon_y, selected ? icons->selected : icons->wheel, scale, metrics.shadow_offset, &icon_w, &icon_h);

        if (selected)
            CG_WeaponBar_DrawSelectionScaled(icon_x, icon_y, icon_w, icon_h, scale);

        if (weap->ammo_index >= 0) {
            int count = CG_Wheel_GetAmmoCount(ps, weap->ammo_index);
            rgba_t color = CG_WeaponBar_CountColor(weap, selected, count);
            char count_text[16];
            CG_WeaponBar_FormatCount(count, count_text, sizeof(count_text));

            int text_x = tile_x + metrics.inset + metrics.max_icon_w + metrics.pad;
            int text_y = tile_y + ((metrics.tile_h - metrics.char_h) / 2);

            CG_WeaponBar_DrawScaledString(text_x, text_y, scale, text_flags, color, count_text);
        }
    }
}

static void CG_WeaponBar_DrawQ2RTimed(const player_state_t *ps)
{
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;
    CG_WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int bar_w = (int)cg_wheel.weapon_bar.num_slots * (WEAPON_BAR_ICON_SIZE + WEAPON_BAR_PAD);
    int center_x = safe_x + (safe_w / 2);
    int bar_x = center_x - (bar_w / 2);
    int bar_y = safe_y + CG_Rint(safe_h * (wb_screen_frac_y ? wb_screen_frac_y->value : WEAPON_BAR_CENTER_FRAC_Y));
    float ammo_scale = (wb_ammo_scale ? wb_ammo_scale->value : 0.66f);
    int max_icon_h = 0;

    CG_WeaponBar_GetMaxIconSize(1.0f, nullptr, &max_icon_h);
    bar_x = CG_WeaponBar_ClampStartInRange(bar_x, bar_w, safe_x, safe_x + safe_w);
    bar_y = CG_WeaponBar_ClampStartInRange(bar_y, max_icon_h, safe_y, safe_y + safe_h);

    for (int i = 0; i < (int)cg_wheel.weapon_bar.num_slots; i++, bar_x += WEAPON_BAR_ICON_SIZE + WEAPON_BAR_PAD) {
        bool selected = cg_wheel.weapon_bar.selected == cg_wheel.weapon_bar.slots[i].item_index;
        const cg_wheel_weapon_t *weap = &cg_wheel.data.weapons[cg_wheel.weapon_bar.slots[i].data_id];
        const cg_wheel_icon_t *icons = &weap->icons;

        int icon_w = 0;
        int icon_h = 0;
        CG_WeaponBar_DrawPicShadowScaled(bar_x, bar_y, selected ? icons->selected : icons->wheel, 1.0f, 2, &icon_w, &icon_h);

        if (selected) {
            CG_WeaponBar_DrawSelectionScaled(bar_x, bar_y, icon_w, icon_h, 1.0f);

            const char *localized = cgi.Localize(cgi.get_configString(CS_ITEMS + cg_wheel.weapon_bar.slots[i].item_index), nullptr, 0);
            if (localized && *localized) {
                int name_y = max(safe_y, bar_y - 16);
                CG_WeaponBar_DrawScaledString(center_x, name_y, 1.0f, UI_CENTER | UI_DROPSHADOW, rgba_white, localized);
            }
        }

        if (weap->ammo_index >= 0) {
            int count = CG_Wheel_GetAmmoCount(ps, weap->ammo_index);
            rgba_t color = CG_WeaponBar_CountColor(weap, selected, count);

            CG_DrawScaledInt(bar_x + (WEAPON_BAR_ICON_SIZE / 2),
                             bar_y + WEAPON_BAR_ICON_SIZE + 2,
                             ammo_scale,
                             UI_DROPSHADOW | UI_CENTER,
                             color,
                             count);
        }
    }
}

static void CG_WeaponBar_DrawQ3Timed(const player_state_t *ps, int selected_item)
{
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;
    CG_WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int bar_y = safe_y + CG_Rint(safe_h * WEAPON_BAR_CENTER_FRAC_Y);
    int center_x = safe_x + (safe_w / 2);

    CG_WeaponBar_DrawHorizontalScaled(ps, bar_y, selected_item, false, false, 1.0f);

    if (selected_item >= 0) {
        const char *localized = cgi.Localize(cgi.get_configString(CS_ITEMS + selected_item), nullptr, 0);
        if (localized && *localized) {
            int name_y = max(safe_y, bar_y - WEAPON_BAR_NAME_OFFSET);
            CG_WeaponBar_DrawScaledString(center_x, name_y, 1.0f, UI_CENTER | UI_DROPSHADOW, rgba_white, localized);
        }
    }
}

void CG_WeaponBar_Draw(const player_state_t *ps, const vrect_t &hud_vrect, const vrect_t &hud_safe, int32_t scale)
{
    if (!ps)
        return;

    cg_wheel.hud_vrect = hud_vrect;
    cg_wheel.hud_safe = hud_safe;
    cg_wheel.hud_valid = true;

    int mode = CG_WeaponBar_GetMode();
    CG_WeaponBar_ApplyMode(mode);

    if (cg_wheel.wheel.state != WHEEL_CLOSED || cg_wheel.wheel.timer > 0.0f)
        return;

    if (mode == WEAPON_BAR_DISABLED)
        return;

    if (mode == WEAPON_BAR_TIMED_Q2R && cg_wheel.weapon_bar.state != WHEEL_OPEN)
        return;

    if (mode == WEAPON_BAR_TIMED_Q3 && cg_wheel.weapon_bar.state != WHEEL_OPEN)
        return;

    bool prefer_active = (mode == WEAPON_BAR_STATIC_LEFT || mode == WEAPON_BAR_STATIC_RIGHT || mode == WEAPON_BAR_STATIC_CENTER);
    bool strict_selection = (mode == WEAPON_BAR_TIMED_Q2R);

    if (!CG_WeaponBar_Populate(ps, prefer_active, strict_selection))
        return;

    int selected_item = cg_wheel.weapon_bar.selected;
    if (prefer_active || selected_item < 0) {
        int active = ps->stats[STAT_ACTIVE_WEAPON];
        if (active >= 0 && active < cg_wheel.data.num_weapons)
            selected_item = cg_wheel.data.weapons[active].item_index;
    }

    switch (mode) {
    case WEAPON_BAR_STATIC_LEFT:
        CG_WeaponBar_DrawStaticTilesVertical(ps, selected_item, false);
        break;
    case WEAPON_BAR_STATIC_RIGHT:
        CG_WeaponBar_DrawStaticTilesVertical(ps, selected_item, true);
        break;
    case WEAPON_BAR_STATIC_CENTER:
        CG_WeaponBar_DrawStaticTilesHorizontal(ps, selected_item);
        break;
    case WEAPON_BAR_TIMED_Q3:
        CG_WeaponBar_DrawQ3Timed(ps, selected_item);
        break;
    case WEAPON_BAR_TIMED_Q2R:
        CG_WeaponBar_DrawQ2RTimed(ps);
        break;
    default:
        break;
    }
}

void CG_WeaponBar_Input(const player_state_t *ps, button_t *cmd_buttons)
{
    if (!ps)
        return;

    int mode = CG_WeaponBar_GetMode();
    CG_WeaponBar_ApplyMode(mode);

    if (mode == WEAPON_BAR_TIMED_Q2R) {
        if (cg_wheel.weapon_bar.state != WHEEL_OPEN) {
            if (cg_wheel.weapon_bar.state == WHEEL_CLOSING && CG_Wheel_Now() >= cg_wheel.weapon_bar.close_time)
                cg_wheel.weapon_bar.state = WHEEL_CLOSED;

            return;
        }

        if (!CG_WeaponBar_Populate(ps, false, true)) {
            CG_WeaponBar_Close();
            return;
        }

        if (cmd_buttons)
            *cmd_buttons |= BUTTON_HOLSTER;

        if (CG_Wheel_Now() >= cg_wheel.weapon_bar.close_time || (cmd_buttons && (*cmd_buttons & BUTTON_ATTACK))) {
            int active = ps->stats[STAT_ACTIVE_WEAPON];
            if (active >= 0 && active < cg_wheel.data.num_weapons &&
                cg_wheel.weapon_bar.selected == cg_wheel.data.weapons[active].item_index) {
                CG_WeaponBar_Close();
                return;
            }

            char cmd[64];
            CG_Snprintf(cmd, sizeof(cmd), "use_index_only %i\n", cg_wheel.weapon_bar.selected);
            cgi.AddCommandString(cmd);
            cg_wheel.weapon_bar.state = WHEEL_CLOSING;
            cg_wheel.weapon_lock_time = CG_Wheel_ClientTime() + (wb_lock_time ? wb_lock_time->integer : 0);
        }

        return;
    }

    if (mode == WEAPON_BAR_DISABLED)
        return;

    bool prefer_active = (mode == WEAPON_BAR_STATIC_LEFT || mode == WEAPON_BAR_STATIC_RIGHT || mode == WEAPON_BAR_STATIC_CENTER);

    if (!CG_WeaponBar_Populate(ps, prefer_active, false)) {
        CG_WeaponBar_Close();
        return;
    }

    if (mode == WEAPON_BAR_TIMED_Q3 && cg_wheel.weapon_bar.state == WHEEL_OPEN &&
        CG_Wheel_Now() >= cg_wheel.weapon_bar.close_time) {
        CG_WeaponBar_Close();
    }
}

void CG_WeaponBar_ClearInput(void)
{
    if (CG_WeaponBar_GetMode() != WEAPON_BAR_TIMED_Q2R)
        return;

    if (cg_wheel.weapon_bar.state == WHEEL_CLOSING) {
        cg_wheel.weapon_bar.state = WHEEL_CLOSED;
        float frame_time = cgi.CL_FrameTime ? cgi.CL_FrameTime() : cgi.frameTimeSec;
        uint64_t delay = (uint64_t)CG_Rint(max(0.0f, frame_time) * 2000.0f);
        cg_wheel.weapon_bar.close_time = CG_Wheel_Now() + delay;
    }
}

static void CG_WeaponBar_Cycle(int offset)
{
    const player_state_t *ps = cg_wheel.last_ps;
    if (!ps)
        return;

    int mode = CG_WeaponBar_GetMode();
    CG_WeaponBar_ApplyMode(mode);

    if (mode == WEAPON_BAR_TIMED_Q2R) {
        if (cg_wheel.weapon_bar.state != WHEEL_OPEN) {
            if (cg_wheel.weapon_bar.state == WHEEL_CLOSING && CG_Wheel_Now() >= cg_wheel.weapon_bar.close_time)
                cg_wheel.weapon_bar.state = WHEEL_CLOSED;
            CG_WeaponBar_Open(ps);
        } else if (!CG_WeaponBar_Populate(ps, false, true)) {
            CG_WeaponBar_Close();
            return;
        }

        CG_WeaponBar_AdvanceSelection(offset);
        cg_wheel.weapon_bar.close_time = CG_Wheel_Now() + (wb_timeout ? wb_timeout->integer : 0);
        return;
    }

    if (mode == WEAPON_BAR_DISABLED)
        return;

    if (!CG_WeaponBar_Populate(ps, true, false)) {
        CG_WeaponBar_Close();
        return;
    }

    int previous = cg_wheel.weapon_bar.selected;
    if (!CG_WeaponBar_AdvanceSelection(offset))
        return;

    if (cg_wheel.weapon_bar.selected != previous) {
        char cmd[64];
        CG_Snprintf(cmd, sizeof(cmd), "use_index_only %i\n", cg_wheel.weapon_bar.selected);
        cgi.AddCommandString(cmd);
        cg_wheel.weapon_lock_time = CG_Wheel_ClientTime() + (wb_lock_time ? wb_lock_time->integer : 0);
    }

    if (mode == WEAPON_BAR_TIMED_Q3) {
        cg_wheel.weapon_bar.state = WHEEL_OPEN;
        cg_wheel.weapon_bar.close_time = CG_Wheel_Now() + (wb_timeout ? wb_timeout->integer : 0);
    }
}

void CG_WeaponBar_Precache(void)
{
    Q_strlcpy(cg_wheel.assets.weapon_bar_selected, "carousel/selected", sizeof(cg_wheel.assets.weapon_bar_selected));
    cgi.Draw_RegisterPic(cg_wheel.assets.weapon_bar_selected);
    cgi.Draw_GetPicSize(&cg_wheel.assets.weapon_bar_selected_w,
                        &cg_wheel.assets.weapon_bar_selected_h,
                        cg_wheel.assets.weapon_bar_selected);
}

void CG_WeaponBar_Init(void)
{
    wb_screen_frac_y = cgi.cvar("wc_screen_frac_y", "0.72", CVAR_NOFLAGS);
    wb_timeout = cgi.cvar("wc_timeout", "400", CVAR_NOFLAGS);
    wb_lock_time = cgi.cvar("wc_lock_time", "300", CVAR_NOFLAGS);
    wb_ammo_scale = cgi.cvar("wc_ammo_scale", "0.66", CVAR_NOFLAGS);
    wb_mode = cgi.cvar("cl_weaponBar", "5", CVAR_ARCHIVE);

    cg_wheel.weapon_bar.state = WHEEL_CLOSED;
    cg_wheel.weapon_bar.selected = -1;
    cg_wheel.weapon_bar.close_time = 0;
    cg_wheel.weapon_bar.num_slots = 0;
    weapon_bar_last_mode = WEAPON_BAR_TIMED_Q2R;
    cg_wheel.weapon_lock_time = 0;
}
