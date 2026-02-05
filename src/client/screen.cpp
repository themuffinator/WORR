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
// cl_scrn.c -- master for renderer, status bar, console, chat, notify, etc

#include "client.h"
#include "client/font.h"
#include "client/ui_font.h"

static cvar_t   *scr_viewsize;
static cvar_t   *scr_showpause;
#if USE_DEBUG
static cvar_t   *scr_showstats;
static cvar_t   *scr_showpmove;
#endif
static cvar_t   *scr_showturtle;
static cvar_t   *scr_showpause_legacy;
#if USE_DEBUG
static cvar_t   *scr_showstats_legacy;
static cvar_t   *scr_showpmove_legacy;
#endif
static cvar_t   *scr_showturtle_legacy;

static cvar_t   *scr_netgraph;
static cvar_t   *scr_timegraph;
static cvar_t   *scr_debuggraph;
static cvar_t   *scr_graphheight;
static cvar_t   *scr_graphscale;
static cvar_t   *scr_graphshift;

static cvar_t   *scr_draw2d;
static cvar_t   *scr_lag_x;
static cvar_t   *scr_lag_y;
static cvar_t   *scr_lag_draw;
static cvar_t   *scr_lag_min;
static cvar_t   *scr_lag_max;
static cvar_t   *cg_lagometer;
static cvar_t   *scr_alpha;
static cvar_t   *scr_draw2d_legacy;
static cvar_t   *scr_lag_x_legacy;
static cvar_t   *scr_lag_y_legacy;
static cvar_t   *scr_lag_draw_legacy;
static cvar_t   *scr_lag_min_legacy;
static cvar_t   *scr_lag_max_legacy;
static cvar_t   *scr_alpha_legacy;

static cvar_t   *scr_demobar;
static cvar_t   *scr_font;
static cvar_t   *scr_scale;
static cvar_t   *cl_font_skip_virtual_scale;
static cvar_t   *scr_demobar_legacy;
static cvar_t   *scr_font_legacy;
static cvar_t   *scr_scale_legacy;
static const float k_scr_ttf_letter_spacing = 0.06f;
static const char k_scr_ui_font_path[] = "fonts/AtkinsonHyperLegible-Regular.otf";

static cvar_t   *scr_crosshair;

static cvar_t   *scr_chathud;
static cvar_t   *scr_chathud_lines;
static cvar_t   *scr_chathud_time;
static cvar_t   *scr_chathud_x;
static cvar_t   *scr_chathud_y;
static cvar_t   *scr_chathud_legacy;
static cvar_t   *scr_chathud_lines_legacy;
static cvar_t   *scr_chathud_time_legacy;
static cvar_t   *scr_chathud_x_legacy;
static cvar_t   *scr_chathud_y_legacy;

static cvar_t   *cl_crosshair_brightness;
static cvar_t   *cl_crosshair_color;
static cvar_t   *cl_crosshair_health;
static cvar_t   *cl_crosshair_hit_color;
static cvar_t   *cl_crosshair_hit_style;
static cvar_t   *cl_crosshair_hit_time;
static cvar_t   *cl_crosshair_pulse;
static cvar_t   *cl_crosshair_size;
static cvar_t   *cl_crosshair_brightness_legacy;
static cvar_t   *cl_crosshair_color_legacy;
static cvar_t   *cl_crosshair_health_legacy;
static cvar_t   *cl_crosshair_hit_color_legacy;
static cvar_t   *cl_crosshair_hit_style_legacy;
static cvar_t   *cl_crosshair_hit_time_legacy;
static cvar_t   *cl_crosshair_pulse_legacy;
static cvar_t   *cl_crosshair_size_legacy;

static cvar_t   *ch_x;
static cvar_t   *ch_y;

static cvar_t   *scr_hit_marker_time;
static cvar_t   *scr_hit_marker_time_legacy;

static cvar_t   *scr_damage_indicators;
static cvar_t   *scr_damage_indicator_time;
static cvar_t   *scr_damage_indicators_legacy;
static cvar_t   *scr_damage_indicator_time_legacy;

static cvar_t   *scr_pois;
static cvar_t   *scr_poi_edge_frac;
static cvar_t   *scr_poi_max_scale;
static cvar_t   *scr_pois_legacy;
static cvar_t   *scr_poi_edge_frac_legacy;
static cvar_t   *scr_poi_max_scale_legacy;

static cvar_t   *scr_safe_zone;
static cvar_t   *scr_safe_zone_legacy;

static void scr_font_changed(cvar_t *self);
static void scr_scale_changed(cvar_t *self);
static void cl_font_skip_virtual_scale_changed(cvar_t *self);
static void scr_ui_font_reload(void);

// nb: this is dumb but C doesn't allow
// `(T) { }` to count as a constant
const color_t colorTable[8] = {
    { .u32 = COLOR_U32_BLACK }, { .u32 = COLOR_U32_RED }, { .u32 = COLOR_U32_GREEN }, { .u32 = COLOR_U32_YELLOW },
    { .u32 = COLOR_U32_BLUE }, { .u32 = COLOR_U32_CYAN }, { .u32 = COLOR_U32_MAGENTA }, { .u32 = COLOR_U32_WHITE }
};

#define CROSSHAIR_PULSE_TIME_MS 200
#define CROSSHAIR_PULSE_SMALL   0.25f
#define CROSSHAIR_PULSE_LARGE   0.5f

static unsigned scr_crosshair_pulse_time;
static int scr_last_pickup_icon;
static int scr_last_pickup_string;

static const color_t ql_crosshair_colors[26] = {
    COLOR_RGBA(255, 0, 0, 255),
    COLOR_RGBA(255, 64, 0, 255),
    COLOR_RGBA(255, 128, 0, 255),
    COLOR_RGBA(255, 192, 0, 255),
    COLOR_RGBA(255, 255, 0, 255),
    COLOR_RGBA(192, 255, 0, 255),
    COLOR_RGBA(128, 255, 0, 255),
    COLOR_RGBA(64, 255, 0, 255),
    COLOR_RGBA(0, 255, 0, 255),
    COLOR_RGBA(0, 255, 64, 255),
    COLOR_RGBA(0, 255, 128, 255),
    COLOR_RGBA(0, 255, 192, 255),
    COLOR_RGBA(0, 255, 255, 255),
    COLOR_RGBA(0, 192, 255, 255),
    COLOR_RGBA(0, 128, 255, 255),
    COLOR_RGBA(0, 64, 255, 255),
    COLOR_RGBA(0, 0, 255, 255),
    COLOR_RGBA(64, 0, 255, 255),
    COLOR_RGBA(128, 0, 255, 255),
    COLOR_RGBA(192, 0, 255, 255),
    COLOR_RGBA(255, 0, 255, 255),
    COLOR_RGBA(255, 0, 192, 255),
    COLOR_RGBA(255, 0, 128, 255),
    COLOR_RGBA(255, 0, 64, 255),
    COLOR_RGBA(255, 255, 255, 255),
    COLOR_RGBA(128, 128, 128, 255)
};

cl_scr_t scr;

/*
===============================================================================

UTILS

===============================================================================
*/

static void SCR_UpdateVirtualScreen(void)
{
    float scale_x = (float)r_config.width / VIRTUAL_SCREEN_WIDTH;
    float scale_y = (float)r_config.height / VIRTUAL_SCREEN_HEIGHT;
    float base_scale = max(scale_x, scale_y);
    int base_scale_int = (int)base_scale;

    if (base_scale_int < 1)
        base_scale_int = 1;

    scr.virtual_width = r_config.width / base_scale_int;
    scr.virtual_height = r_config.height / base_scale_int;
    if (scr.virtual_width < 1)
        scr.virtual_width = 1;
    if (scr.virtual_height < 1)
        scr.virtual_height = 1;

    scr.virtual_scale = (float)base_scale_int;
}

static int SCR_GetBaseScaleInt(void)
{
    int base_scale_int = (int)scr.virtual_scale;

    if (base_scale_int < 1) {
        float scale_x = (float)r_config.width / VIRTUAL_SCREEN_WIDTH;
        float scale_y = (float)r_config.height / VIRTUAL_SCREEN_HEIGHT;
        float base_scale = max(scale_x, scale_y);
        base_scale_int = (int)base_scale;
    }

    if (base_scale_int < 1)
        base_scale_int = 1;

    return base_scale_int;
}

static int SCR_GetUiScaleInt(void)
{
    int base_scale_int = SCR_GetBaseScaleInt();
    float extra_scale = 1.0f;

    if (scr_scale && scr_scale->value)
        extra_scale = Cvar_ClampValue(scr_scale, 0.25f, 10.0f);

    int ui_scale_int = (int)((float)base_scale_int * extra_scale);

    if (ui_scale_int < 1)
        ui_scale_int = 1;

    return ui_scale_int;
}

static float SCR_GetFontPixelScale(void)
{
    if (!cl_font_skip_virtual_scale)
        cl_font_skip_virtual_scale =
            Cvar_Get("cl_font_skip_virtual_scale", "0", CVAR_ARCHIVE);

    int base_scale_int = SCR_GetBaseScaleInt();
    float hud_scale = scr.hud_scale > 0.0f ? scr.hud_scale : 1.0f;

    if (cl_font_skip_virtual_scale && cl_font_skip_virtual_scale->integer)
        return 1.0f / hud_scale;

    return (float)base_scale_int / hud_scale;
}

static void cl_font_skip_virtual_scale_changed(cvar_t *self)
{
    if (!self)
        return;
    if (!scr.initialized || !cls.ref_initialized)
        return;

    Con_CheckResize();
    UI_FontModeChanged();
    if (scr_font)
        scr_font_changed(scr_font);
}

static bool SCR_UseScrFont(qhandle_t font)
{
    if (!scr.ui_font)
        return false;
    return !font || font == scr.ui_font_pic;
}

static int SCR_MeasureFontString(const char *text, size_t max_chars)
{
    if (!text || !*text)
        return 0;

    if (scr.ui_font)
        return Font_MeasureString(scr.ui_font, 1, 0, max_chars, text, nullptr);

    size_t len = strlen(text);
    if (len > max_chars)
        len = max_chars;
    return (int)Com_StrlenNoColor(text, len) * CONCHAR_WIDTH;
}

int SCR_MeasureString(const char *text, size_t max_chars)
{
    return SCR_MeasureFontString(text, max_chars);
}

/*
==============
SCR_DrawStringStretch
==============
*/
int SCR_DrawStringStretch(int x, int y, int scale, int flags, size_t maxlen,
                          const char *s, color_t color, qhandle_t font)
{
    size_t len = strlen(s);

    if (len > maxlen) {
        len = maxlen;
    }

    if (SCR_UseScrFont(font)) {
        int text_width = Font_MeasureString(scr.ui_font, scale, flags, len, s, nullptr);
        if ((flags & UI_CENTER) == UI_CENTER) {
            x -= text_width / 2;
        } else if (flags & UI_RIGHT) {
            x -= text_width;
        }
        return Font_DrawString(scr.ui_font, x, y, scale, flags, len, s, color);
    }

    size_t visible_len = Com_StrlenNoColor(s, len);
    if ((flags & UI_CENTER) == UI_CENTER) {
        x -= (visible_len * CONCHAR_WIDTH * scale) / 2;
    } else if (flags & UI_RIGHT) {
        x -= visible_len * CONCHAR_WIDTH * scale;
    }

    return R_DrawStringStretch(x, y, scale, flags, maxlen, s, color, font);
}


/*
==============
SCR_DrawStringMultiStretch
==============
*/
void SCR_DrawStringMultiStretch(int x, int y, int scale, int flags, size_t maxlen,
                                const char *s, color_t color, qhandle_t font)
{
    const char *p;
    size_t  len;
    int     last_x = x;
    int     last_y = y;

    while (*s && maxlen) {
        p = strchr(s, '\n');
        if (!p) {
            last_x = SCR_DrawStringStretch(x, y, scale, flags, maxlen, s, color, font);
            last_y = y;
            break;
        }

        len = min(p - s, maxlen);
        last_x = SCR_DrawStringStretch(x, y, scale, flags, len, s, color, font);
        last_y = y;
        maxlen -= len;

        y += CONCHAR_HEIGHT * scale;
        s = p + 1;
    }

    if (flags & UI_DRAWCURSOR && com_localTime & BIT(8))
        R_DrawStretchChar(last_x, last_y, CONCHAR_WIDTH * scale, CONCHAR_HEIGHT * scale, flags, 11, color, font);
}

static int SCR_DrawKStringStretch(int x, int y, int scale, int flags, size_t maxlen, const char *s, color_t color, const kfont_t *kfont)
{
    while (*s && maxlen--) {
        x += R_DrawKFontChar(x, y, scale, flags, *s++, color, kfont);
    }
    return x;
}

void SCR_DrawKStringMultiStretch(int x, int y, int scale, int flags, size_t maxlen, const char *s, color_t color, const kfont_t *kfont)
{
    int     last_x = x;
    int     last_y = y;

    while (*s && maxlen) {
        const char *p = strchr(s, '\n');
        if (!p) {
            last_x = SCR_DrawKStringStretch(x, y, scale, flags, maxlen, s, color, kfont);
            last_y = y;
            break;
        }

        int len = min(p - s, maxlen);
        last_x = SCR_DrawKStringStretch(x, y, scale, flags, len, s, color, kfont);
        last_y = y;
        maxlen -= len;

        y += CONCHAR_HEIGHT * scale;
        s = p + 1;
    }

    if (flags & UI_DRAWCURSOR && com_localTime & BIT(8))
        R_DrawKFontChar(last_x, last_y, scale, flags, 11, color, kfont);
}

typedef struct {
    qhandle_t pic;
    int w;
    int h;
    bool tried;
} bind_icon_cache_t;

static bind_icon_cache_t scr_bind_icon_cache[256];

static int SCR_MapKeynumToMouseIcon(int keynum)
{
    switch (keynum) {
    case K_MOUSE1:
        return 1;
    case K_MOUSE2:
        return 2;
    case K_MOUSE3:
        return 0;
    case K_MOUSE4:
        return 5;
    case K_MOUSE5:
        return 6;
    case K_MOUSE6:
        return 7;
    case K_MOUSE7:
        return 8;
    case K_MOUSE8:
        return 9;
    case K_MWHEELUP:
        return 3;
    case K_MWHEELDOWN:
        return 4;
    default:
        return -1;
    }
}

static int SCR_MapKeynumToKeyboardIcon(int keynum)
{
    switch (keynum) {
    case K_BACKSPACE:
        return 8;
    case K_TAB:
        return 9;
    case K_ENTER:
        return 13;
    case K_PAUSE:
        return 271;
    case K_ESCAPE:
        return 27;
    case K_SPACE:
        return 32;
    case K_DEL:
        return 275;
    case K_CAPSLOCK:
        return 256;
    case K_F1:
        return 257;
    case K_F2:
        return 258;
    case K_F3:
        return 259;
    case K_F4:
        return 260;
    case K_F5:
        return 261;
    case K_F6:
        return 262;
    case K_F7:
        return 263;
    case K_F8:
        return 264;
    case K_F9:
        return 265;
    case K_F10:
        return 266;
    case K_F11:
        return 267;
    case K_F12:
        return 268;
    case K_PRINTSCREEN:
        return 269;
    case K_SCROLLOCK:
        return 270;
    case K_INS:
        return 272;
    case K_HOME:
        return 273;
    case K_PGUP:
        return 274;
    case K_END:
        return 276;
    case K_PGDN:
        return 277;
    case K_RIGHTARROW:
        return 278;
    case K_LEFTARROW:
        return 279;
    case K_DOWNARROW:
        return 280;
    case K_UPARROW:
        return 281;
    case K_NUMLOCK:
        return 282;
    case K_KP_SLASH:
        return 283;
    case K_KP_MULTIPLY:
        return 42;
    case K_KP_MINUS:
        return 285;
    case K_KP_PLUS:
        return 286;
    case K_KP_ENTER:
        return 287;
    case K_KP_END:
        return 288;
    case K_KP_DOWNARROW:
        return 289;
    case K_KP_PGDN:
        return 290;
    case K_KP_LEFTARROW:
        return 291;
    case K_KP_5:
        return 292;
    case K_KP_RIGHTARROW:
        return 293;
    case K_KP_HOME:
        return 294;
    case K_KP_UPARROW:
        return 295;
    case K_KP_PGUP:
        return 296;
    case K_KP_INS:
        return 297;
    case K_KP_DEL:
        return 298;
    case K_CTRL:
    case K_LCTRL:
        return 299;
    case K_SHIFT:
    case K_LSHIFT:
        return 300;
    case K_ALT:
    case K_LALT:
        return 301;
    case K_RCTRL:
        return 302;
    case K_RSHIFT:
        return 303;
    case K_RALT:
        return 304;
    default:
        break;
    }

    if (keynum >= K_ASCIIFIRST && keynum <= K_ASCIILAST)
        return keynum;

    return -1;
}

static bool SCR_BuildGamepadIconPath(int keynum, char *out_path, size_t out_size)
{
    if (keynum < K_GAMEPAD_FIRST || keynum > K_GAMEPAD_LAST)
        return false;

    int index = keynum - K_GAMEPAD_FIRST;
    if (index < 0)
        return false;
    Q_snprintf(out_path, out_size, "/gfx/controller/generic/f%04x.png", index);
    return true;
}

static bool SCR_BuildBindIconPath(int keynum, char *out_path, size_t out_size)
{
    int mouse_icon = SCR_MapKeynumToMouseIcon(keynum);
    if (mouse_icon >= 0) {
        Q_snprintf(out_path, out_size, "/gfx/controller/mouse/f000%i.png", mouse_icon);
        return true;
    }

    if (SCR_BuildGamepadIconPath(keynum, out_path, out_size)) {
        return true;
    }

    int key_icon = SCR_MapKeynumToKeyboardIcon(keynum);
    if (key_icon >= 0) {
        Q_snprintf(out_path, out_size, "/gfx/controller/keyboard/%i.png", key_icon);
        return true;
    }

    return false;
}

bool SCR_GetBindIconForKey(int keynum, qhandle_t *pic, int *w, int *h)
{
    if (keynum < 0 || keynum > 255)
        return false;

    bind_icon_cache_t *entry = &scr_bind_icon_cache[keynum];
    if (!entry->tried) {
        entry->tried = true;
        char path[MAX_QPATH];

        if (SCR_BuildBindIconPath(keynum, path, sizeof(path))) {
            entry->pic = R_RegisterPic(path);
            if (entry->pic) {
                R_GetPicSize(&entry->w, &entry->h, entry->pic);
            }
        }

        if (!entry->pic || entry->w <= 0 || entry->h <= 0) {
            entry->pic = 0;
            entry->w = 0;
            entry->h = 0;
        }
    }

    if (!entry->pic)
        return false;

    if (pic)
        *pic = entry->pic;
    if (w)
        *w = entry->w;
    if (h)
        *h = entry->h;

    return true;
}

int SCR_DrawBindIcon(const char *binding, int x, int y, int size, color_t color, const char **out_keyname)
{
    if (out_keyname)
        *out_keyname = NULL;

    if (!binding || !*binding)
        return 0;

    int keynum = Key_EnumBindings(0, binding);
    if (keynum < 0)
        return 0;

    if (out_keyname)
        *out_keyname = Key_KeynumToString(keynum);

    qhandle_t pic;
    int w, h;
    if (!SCR_GetBindIconForKey(keynum, &pic, &w, &h))
        return 0;

    int draw_h = max(1, size);
    float scale = h > 0 ? ((float)draw_h / (float)h) : 1.0f;
    int draw_w = max(1, Q_rint(w * scale));

    R_DrawStretchPic(x, y, draw_w, draw_h, color, pic);
    return draw_w;
}


/*
=================
SCR_FadeAlpha
=================
*/
float SCR_FadeAlpha(unsigned startTime, unsigned visTime, unsigned fadeTime)
{
    float alpha;
    unsigned timeLeft, delta = cls.realtime - startTime;

    if (delta >= visTime) {
        return 0;
    }

    if (fadeTime > visTime) {
        fadeTime = visTime;
    }

    alpha = 1;
    timeLeft = visTime - delta;
    if (timeLeft < fadeTime) {
        alpha = (float)timeLeft / fadeTime;
    }

    return alpha;
}

bool SCR_ParseColor(const char *s, color_t *color)
{
    int i;
    int c[8];

    // parse generic color
    if (*s == '#') {
        s++;
        for (i = 0; s[i]; i++) {
            if (i == 8) {
                return false;
            }
            c[i] = Q_charhex(s[i]);
            if (c[i] == -1) {
                return false;
            }
        }

        switch (i) {
        case 3:
            color->r = c[0] | (c[0] << 4);
            color->g = c[1] | (c[1] << 4);
            color->b = c[2] | (c[2] << 4);
            color->a = 255;
            break;
        case 6:
            color->r = c[1] | (c[0] << 4);
            color->g = c[3] | (c[2] << 4);
            color->b = c[5] | (c[4] << 4);
            color->a = 255;
            break;
        case 8:
            color->r = c[1] | (c[0] << 4);
            color->g = c[3] | (c[2] << 4);
            color->b = c[5] | (c[4] << 4);
            color->a = c[7] | (c[6] << 4);
            break;
        default:
            return false;
        }

        return true;
    }

    // parse name or index
    i = Com_ParseColor(s);
    if (i >= q_countof(colorTable)) {
        return false;
    }

    *color = colorTable[i];
    return true;
}

static color_t SCR_ApplyCrosshairBrightness(color_t color)
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

static color_t SCR_GetCrosshairPaletteColor(int index)
{
    index = Q_clip(index, 1, (int)q_countof(ql_crosshair_colors));
    return ql_crosshair_colors[index - 1];
}

static color_t SCR_GetCrosshairDamageColor(int damage)
{
    float t = Q_clipf((float)damage / 100.0f, 0.0f, 1.0f);

    return COLOR_RGBA(
        Q_rint(255.0f * t),
        Q_rint(255.0f * (1.0f - t)),
        0,
        255);
}

static float SCR_CalcPickupPulseScale(unsigned start_time, unsigned duration_ms)
{
    if (!start_time || !duration_ms)
        return 1.0f;
    if (cls.realtime <= start_time)
        return 1.0f;

    unsigned delta = cls.realtime - start_time;
    if (delta >= duration_ms)
        return 1.0f;

    float frac = (float)delta / (float)duration_ms;
    return 1.0f + frac;
}

static float SCR_CalcCrosshairPulseScale(unsigned start_time, unsigned duration_ms, float amplitude)
{
    if (!start_time || !duration_ms || amplitude <= 0.0f)
        return 1.0f;
    if (cls.realtime <= start_time)
        return 1.0f;

    unsigned delta = cls.realtime - start_time;
    if (delta >= duration_ms)
        return 1.0f;

    float frac = (float)delta / (float)duration_ms;
    float falloff = 1.0f - (frac * frac);
    return 1.0f + (amplitude * falloff);
}

void SCR_NotifyPickupPulse(void)
{
    if (cgame && cgame->DrawCrosshair && cgame->NotifyPickupPulse) {
        cgame->NotifyPickupPulse(0);
        return;
    }

    if (!cl_crosshair_pulse || !cl_crosshair_pulse->integer)
        return;

    scr_crosshair_pulse_time = cls.realtime;
}

static void SCR_UpdateCrosshairPickupPulse(void)
{
    int pickup_icon = cl.frame.ps.stats[STAT_PICKUP_ICON];
    int pickup_string = cl.frame.ps.stats[STAT_PICKUP_STRING];

    if (pickup_icon != scr_last_pickup_icon || pickup_string != scr_last_pickup_string) {
        if (pickup_icon)
            SCR_NotifyPickupPulse();
        scr_last_pickup_icon = pickup_icon;
        scr_last_pickup_string = pickup_string;
    }
}

/*
===============================================================================

BAR GRAPHS

===============================================================================
*/

/*
==============
SCR_AddNetgraph

A new packet was just parsed
==============
*/
void SCR_AddNetgraph(void)
{
    int         i, color;
    unsigned    ping;

    // if using the debuggraph for something else, don't
    // add the net lines
    if (scr_debuggraph->integer || scr_timegraph->integer)
        return;

    for (i = 0; i < cls.netchan.dropped; i++)
        SCR_DebugGraph(30, 0x40);

    for (i = 0; i < cl.suppress_count; i++)
        SCR_DebugGraph(30, 0xdf);

    if (scr_netgraph->integer > 1) {
        ping = msg_read.cursize;
        if (ping < 200)
            color = 61;
        else if (ping < 500)
            color = 59;
        else if (ping < 800)
            color = 57;
        else if (ping < 1200)
            color = 224;
        else
            color = 242;
        ping /= 40;
    } else {
        // see what the latency was on this packet
        i = cls.netchan.incoming_acknowledged & CMD_MASK;
        ping = (cls.realtime - cl.history[i].sent) / 30;
        color = 0xd0;
    }

    SCR_DebugGraph(min(ping, 30), color);
}

#define GRAPH_SAMPLES   4096
#define GRAPH_MASK      (GRAPH_SAMPLES - 1)

static struct {
    float       values[GRAPH_SAMPLES];
    byte        colors[GRAPH_SAMPLES];
    unsigned    current;
} graph;

/*
==============
SCR_DebugGraph
==============
*/
void SCR_DebugGraph(float value, int color)
{
    graph.values[graph.current & GRAPH_MASK] = value;
    graph.colors[graph.current & GRAPH_MASK] = color;
    graph.current++;
}

/*
==============
SCR_DrawDebugGraph
==============
*/
static void SCR_DrawDebugGraph(void)
{
    int     a, y, w, i, h, height;
    float   v, scale, shift;

    scale = scr_graphscale->value;
    shift = scr_graphshift->value;
    height = scr_graphheight->integer;
    if (height < 1)
        return;

    w = scr.hud_width;
    y = scr.hud_height;

    for (a = 0; a < w; a++) {
        i = (graph.current - 1 - a) & GRAPH_MASK;
        v = graph.values[i] * scale + shift;

        if (v < 0)
            v += height * (1 + (int)(-v / height));

        h = (int)v % height;
        R_DrawFill8(w - 1 - a, y - h, 1, h, graph.colors[i]);
    }
}

/*
===============================================================================

DEMO BAR

===============================================================================
*/

static void draw_progress_bar(float progress, bool paused, int framenum)
{
    char buffer[16];
    int w, h;

    w = Q_rint(scr.hud_width * progress);
    h = Q_rint(CONCHAR_HEIGHT / scr.hud_scale);

    scr.hud_height -= h;

    R_DrawFill8(0, scr.hud_height, w, h, 4);
    R_DrawFill8(w, scr.hud_height, scr.hud_width - w, h, 0);

    R_SetScale(scr.hud_scale);

    w = Q_rint(scr.hud_width * scr.hud_scale);
    h = Q_rint(scr.hud_height * scr.hud_scale);

    Q_scnprintf(buffer, sizeof(buffer), "%.f%%", progress * 100);
    SCR_DrawString(w / 2, h, UI_CENTER, COLOR_WHITE, buffer);

    if (scr_demobar->integer > 1) {
        int sec = framenum / BASE_FRAMERATE;
        int min = sec / 60; sec %= 60;

        Q_scnprintf(buffer, sizeof(buffer), "%d:%02d.%d", min, sec, framenum % BASE_FRAMERATE);
        SCR_DrawString(0, h, 0, COLOR_WHITE, buffer);
    }

    if (paused) {
        SCR_DrawString(w, h, UI_RIGHT, COLOR_WHITE, "[PAUSED]");
    }

    R_SetScale(1.0f);
}

static void SCR_DrawDemo(void)
{
#if USE_MVD_CLIENT
    float progress;
    bool paused;
    int framenum;
#endif

    if (!scr_demobar->integer) {
        return;
    }

    if (cls.demo.playback) {
        if (cls.demo.file_size && !cls.demo.compat) {
            draw_progress_bar(
                cls.demo.file_progress,
                sv_paused->integer &&
                cl_paused->integer &&
                scr_showpause->integer == 2,
                cls.demo.frames_read);
        }
        return;
    }

#if USE_MVD_CLIENT
    if (sv_running->integer != ss_broadcast) {
        return;
    }

    if (!MVD_GetDemoStatus(&progress, &paused, &framenum)) {
        return;
    }

    if (sv_paused->integer && cl_paused->integer && scr_showpause->integer == 2) {
        paused = true;
    }

    draw_progress_bar(progress, paused, framenum);
#endif
}

/*
===============================================================================

LAGOMETER

===============================================================================
*/

#define LAG_WIDTH   48
#define LAG_HEIGHT  48

#define LAG_WARN_BIT    BIT(30)
#define LAG_CRIT_BIT    BIT(31)

#define LAG_BASE    0xD5
#define LAG_WARN    0xDC
#define LAG_CRIT    0xF2

static struct {
    unsigned samples[LAG_WIDTH];
    unsigned head;
} lag;

#define LAGOMETER_SAMPLES       128
#define LAGOMETER_RATE_DELAYED  BIT(0)
#define MAX_LAGOMETER_PING      900
#define MAX_LAGOMETER_RANGE     300

typedef struct {
    int frameSamples[LAGOMETER_SAMPLES];
    int frameCount;
    unsigned snapshotFlags[LAGOMETER_SAMPLES];
    int snapshotSamples[LAGOMETER_SAMPLES];
    int snapshotCount;
} lagometer_t;

static lagometer_t lagometer;

void SCR_LagClear(void)
{
    lag.head = 0;
    memset(&lagometer, 0, sizeof(lagometer));
}

static void SCR_LagometerAddFrameInfo(void)
{
    int offset = cl.time - cl.servertime;
    lagometer.frameSamples[lagometer.frameCount & (LAGOMETER_SAMPLES - 1)] = offset;
    lagometer.frameCount++;
}

static void SCR_LagometerAddSnapshotInfo(int ping, unsigned flags)
{
    int index = lagometer.snapshotCount & (LAGOMETER_SAMPLES - 1);
    lagometer.snapshotSamples[index] = ping;
    lagometer.snapshotFlags[index] = flags;
    lagometer.snapshotCount++;
}

void SCR_LagSample(void)
{
    int i = cls.netchan.incoming_acknowledged & CMD_MASK;
    client_history_t *h = &cl.history[i];
    unsigned ping;
    unsigned raw_ping;

    h->rcvd = cls.realtime;
    if (!h->cmdNumber || h->rcvd < h->sent) {
        return;
    }

    ping = h->rcvd - h->sent;
    raw_ping = ping;
    for (i = 0; i < cls.netchan.dropped; i++) {
        lag.samples[lag.head % LAG_WIDTH] = ping | LAG_CRIT_BIT;
        lag.head++;
        SCR_LagometerAddSnapshotInfo(-1, 0);
    }

    unsigned snapshot_flags = 0;
    if (cl.frameflags & FF_SUPPRESSED) {
        snapshot_flags |= LAGOMETER_RATE_DELAYED;
        ping |= LAG_WARN_BIT;
    }
    lag.samples[lag.head % LAG_WIDTH] = ping;
    lag.head++;
    SCR_LagometerAddSnapshotInfo((int)raw_ping, snapshot_flags);
}

static void SCR_LagDraw(int x, int y)
{
    int i, j, v, c, v_min, v_max, v_range;

    v_min = Cvar_ClampInteger(scr_lag_min, 0, LAG_HEIGHT * 10);
    v_max = Cvar_ClampInteger(scr_lag_max, 0, LAG_HEIGHT * 10);

    v_range = v_max - v_min;
    if (v_range < 1)
        return;

    for (i = 0; i < LAG_WIDTH; i++) {
        j = lag.head - i - 1;
        if (j < 0) {
            break;
        }

        v = lag.samples[j % LAG_WIDTH];

        if (v & LAG_CRIT_BIT) {
            c = LAG_CRIT;
        } else if (v & LAG_WARN_BIT) {
            c = LAG_WARN;
        } else {
            c = LAG_BASE;
        }

        v &= ~(LAG_WARN_BIT | LAG_CRIT_BIT);
        v = Q_clip((v - v_min) * LAG_HEIGHT / v_range, 0, LAG_HEIGHT);

        R_DrawFill8(x + LAG_WIDTH - i - 1, y + LAG_HEIGHT - v, 1, v, c);
    }
}

static void SCR_DrawLagometer(int x, int y)
{
    static const color_t lagometer_bg = COLOR_RGBA(0, 0, 0, 96);
    static const color_t lagometer_blue = COLOR_RGBA(0, 0, 255, 255);
    static const color_t lagometer_yellow = COLOR_RGBA(255, 255, 0, 255);
    static const color_t lagometer_green = COLOR_RGBA(0, 255, 0, 255);
    static const color_t lagometer_red = COLOR_RGBA(255, 0, 0, 255);
    int a, i;
    float v;
    float mid;
    float range;
    float vscale;

    R_DrawFill32(x, y, LAG_WIDTH, LAG_HEIGHT, lagometer_bg);

    range = (float)LAG_HEIGHT / 3.0f;
    mid = y + range;
    vscale = range / (float)MAX_LAGOMETER_RANGE;

    for (a = 0; a < LAG_WIDTH; a++) {
        i = (lagometer.frameCount - 1 - a) & (LAGOMETER_SAMPLES - 1);
        v = lagometer.frameSamples[i] * vscale;
        if (v > 0.0f) {
            if (v > range) {
                v = range;
            }
            int h = Q_rint(v);
            if (h > 0) {
                R_DrawFill32(x + LAG_WIDTH - a - 1, Q_rint(mid - v), 1, h, lagometer_yellow);
            }
        } else if (v < 0.0f) {
            v = -v;
            if (v > range) {
                v = range;
            }
            int h = Q_rint(v);
            if (h > 0) {
                R_DrawFill32(x + LAG_WIDTH - a - 1, Q_rint(mid), 1, h, lagometer_blue);
            }
        }
    }

    range = (float)LAG_HEIGHT / 2.0f;
    vscale = range / (float)MAX_LAGOMETER_PING;

    for (a = 0; a < LAG_WIDTH; a++) {
        i = (lagometer.snapshotCount - 1 - a) & (LAGOMETER_SAMPLES - 1);
        v = (float)lagometer.snapshotSamples[i];
        if (v > 0.0f) {
            color_t color = (lagometer.snapshotFlags[i] & LAGOMETER_RATE_DELAYED)
                ? lagometer_yellow
                : lagometer_green;
            v *= vscale;
            if (v > range) {
                v = range;
            }
            int h = Q_rint(v);
            if (h > 0) {
                R_DrawFill32(x + LAG_WIDTH - a - 1, Q_rint(y + LAG_HEIGHT - v), 1, h, color);
            }
        } else if (v < 0.0f) {
            int h = Q_rint(range);
            if (h > 0) {
                R_DrawFill32(x + LAG_WIDTH - a - 1, Q_rint(y + LAG_HEIGHT - range), 1, h, lagometer_red);
            }
        }
    }

    if (!cls.demo.playback) {
        char ping_text[16];
        Q_snprintf(ping_text, sizeof(ping_text), "%ums", cls.measure.ping);
        SCR_DrawString(x + 1, y, 0, COLOR_WHITE, ping_text);
    }

    if (cl_predict && !cl_predict->integer) {
        SCR_DrawString(x + LAG_WIDTH - 1, y, UI_RIGHT, COLOR_WHITE, "snc");
    }
}

static void SCR_DrawNet(color_t base_color)
{
    int x = scr_lag_x->integer;
    int y = scr_lag_y->integer;

    if (x < 0) {
        x += scr.hud_width - LAG_WIDTH + 1;
    }
    if (y < 0) {
        y += scr.hud_height - LAG_HEIGHT + 1;
    }

    // draw ping graph
    if (cg_lagometer && cg_lagometer->integer > 0 && !sv_running->integer) {
        SCR_DrawLagometer(x, y);
    } else if (scr_lag_draw->integer) {
        if (scr_lag_draw->integer > 1) {
            R_DrawFill8(x, y, LAG_WIDTH, LAG_HEIGHT, 4);
        }
        SCR_LagDraw(x, y);
    }

    // draw phone jack
    if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged >= CMD_BACKUP) {
        if ((cls.realtime >> 8) & 3) {
            R_DrawStretchPic(x, y, LAG_WIDTH, LAG_HEIGHT, base_color, scr.net_pic);
        }
    }
}


/*
===============================================================================

DRAW OBJECTS

===============================================================================
*/

typedef struct {
    list_t          entry;
    int             x, y;
    cvar_t          *cvar;
    cmd_macro_t     *macro;
    int             flags;
    color_t         color;
} drawobj_t;

#define FOR_EACH_DRAWOBJ(obj) \
    LIST_FOR_EACH(drawobj_t, obj, &scr_objects, entry)
#define FOR_EACH_DRAWOBJ_SAFE(obj, next) \
    LIST_FOR_EACH_SAFE(drawobj_t, obj, next, &scr_objects, entry)

static LIST_DECL(scr_objects);

static void SCR_Color_g(genctx_t *ctx)
{
    int color;

    for (color = 0; color < COLOR_INDEX_COUNT; color++)
        Prompt_AddMatch(ctx, colorNames[color]);
}

static void SCR_Draw_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        Cvar_Variable_g(ctx);
        Cmd_Macro_g(ctx);
    } else if (argnum == 4) {
        SCR_Color_g(ctx);
    }
}

// draw cl_fps -1 80
static void SCR_Draw_f(void)
{
    int x, y;
    const char *s, *c;
    drawobj_t *obj;
    cmd_macro_t *macro;
    cvar_t *cvar;
    color_t color;
    int flags;
    int argc = Cmd_Argc();

    if (argc == 1) {
        if (LIST_EMPTY(&scr_objects)) {
            Com_Printf("No draw strings registered.\n");
            return;
        }
        Com_Printf("Name               X    Y\n"
                   "--------------- ---- ----\n");
        FOR_EACH_DRAWOBJ(obj) {
            s = obj->macro ? obj->macro->name : obj->cvar->name;
            Com_Printf("%-15s %4d %4d\n", s, obj->x, obj->y);
        }
        return;
    }

    if (argc < 4) {
        Com_Printf("Usage: %s <name> <x> <y> [color]\n", Cmd_Argv(0));
        return;
    }

    color = COLOR_BLACK;
    flags = UI_IGNORECOLOR;

    s = Cmd_Argv(1);
    x = Q_atoi(Cmd_Argv(2));
    y = Q_atoi(Cmd_Argv(3));

    if (x < 0) {
        flags |= UI_RIGHT;
    }

    if (argc > 4) {
        c = Cmd_Argv(4);
        if (!strcmp(c, "alt")) {
            flags |= UI_ALTCOLOR;
        } else if (strcmp(c, "none")) {
            if (!SCR_ParseColor(c, &color)) {
                Com_Printf("Unknown color '%s'\n", c);
                return;
            }
            flags &= ~UI_IGNORECOLOR;
        }
    }

    cvar = NULL;
    macro = Cmd_FindMacro(s);
    if (!macro) {
        cvar = Cvar_WeakGet(s);
    }

    FOR_EACH_DRAWOBJ(obj) {
        if (obj->macro == macro && obj->cvar == cvar) {
            obj->x = x;
            obj->y = y;
            obj->flags = flags;
            obj->color.u32 = color.u32;
            return;
        }
    }

    obj = static_cast<drawobj_t *>(Z_Malloc(sizeof(*obj)));
    obj->x = x;
    obj->y = y;
    obj->cvar = cvar;
    obj->macro = macro;
    obj->flags = flags;
    obj->color.u32 = color.u32;

    List_Append(&scr_objects, &obj->entry);
}

static void SCR_Draw_g(genctx_t *ctx)
{
    drawobj_t *obj;
    const char *s;

    if (LIST_EMPTY(&scr_objects)) {
        return;
    }

    Prompt_AddMatch(ctx, "all");

    FOR_EACH_DRAWOBJ(obj) {
        s = obj->macro ? obj->macro->name : obj->cvar->name;
        Prompt_AddMatch(ctx, s);
    }
}

static void SCR_UnDraw_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        SCR_Draw_g(ctx);
    }
}

static void SCR_UnDraw_f(void)
{
    char *s;
    drawobj_t *obj, *next;
    cmd_macro_t *macro;
    cvar_t *cvar;

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <name>\n", Cmd_Argv(0));
        return;
    }

    if (LIST_EMPTY(&scr_objects)) {
        Com_Printf("No draw strings registered.\n");
        return;
    }

    s = Cmd_Argv(1);
    if (!strcmp(s, "all")) {
        FOR_EACH_DRAWOBJ_SAFE(obj, next) {
            Z_Free(obj);
        }
        List_Init(&scr_objects);
        Com_Printf("Deleted all draw strings.\n");
        return;
    }

    cvar = NULL;
    macro = Cmd_FindMacro(s);
    if (!macro) {
        cvar = Cvar_WeakGet(s);
    }

    FOR_EACH_DRAWOBJ_SAFE(obj, next) {
        if (obj->macro == macro && obj->cvar == cvar) {
            List_Remove(&obj->entry);
            Z_Free(obj);
            return;
        }
    }

    Com_Printf("Draw string '%s' not found.\n", s);
}

static void SCR_DrawObjects(color_t base_color)
{
    char buffer[MAX_QPATH];
    int x, y;
    drawobj_t *obj;

    FOR_EACH_DRAWOBJ(obj) {
        x = obj->x;
        y = obj->y;
        if (x < 0) {
            x += scr.hud_width + 1;
        }
        if (y < 0) {
            y += scr.hud_height - CONCHAR_HEIGHT + 1;
        }

        color_t color = base_color;

        if (!(obj->flags & UI_IGNORECOLOR)) {
            color = obj->color;
        }

        if (obj->macro) {
            obj->macro->function(buffer, sizeof(buffer));
            SCR_DrawString(x, y, obj->flags, color, buffer);
        } else {
            SCR_DrawString(x, y, obj->flags, color, obj->cvar->string);
        }   
    }
}

/*
===============================================================================

CHAT HUD

===============================================================================
*/

#define MAX_NOTIFY_TEXT             150
#define MAX_NOTIFY_LINES            32
#define NOTIFY_LINE_MASK            (MAX_NOTIFY_LINES - 1)
#define NOTIFY_LIFETIME_MS          4000
#define NOTIFY_FADE_MS              200
#define NOTIFY_VISIBLE_LINES        4
#define NOTIFY_VISIBLE_LINES_ACTIVE 8
#define NOTIFY_SCROLL_SPEED         12.0f
#define NOTIFY_STATUSBAR_ICON_HEIGHT (CONCHAR_HEIGHT * 3)
#define NOTIFY_STATUSBAR_OFFSET     (NOTIFY_STATUSBAR_ICON_HEIGHT * 2)

typedef struct {
    char        text[MAX_NOTIFY_TEXT];
    unsigned    time;
    bool        is_chat;
} notifyline_t;

typedef struct {
    notifyline_t *line;
    float alpha;
} notify_draw_t;

typedef struct {
    bool    valid;
    int     x;
    int     max_chars;
    int     width;
    int     viewport_top;
    int     viewport_bottom;
    int     max_visible;
    int     total_lines;
    int     prompt_skip;
    int     input_x;
    int     input_y;
    int     input_w;
    int     input_h;
    int     scrollbar_x;
    int     scrollbar_w;
    int     scroll_track_top;
    int     scroll_track_height;
    int     thumb_top;
    int     thumb_height;
} scr_notify_layout_t;

static notifyline_t scr_notify_lines[MAX_NOTIFY_LINES];
static unsigned     scr_notify_head;
static float        scr_notify_scroll;
static float        scr_notify_scroll_target;
static float        scr_notify_scroll_max;
static bool         scr_notify_drag;
static int          scr_notify_drag_offset;
static int          scr_notify_mouse_x;
static int          scr_notify_mouse_y;
static bool         scr_notify_mouse_valid;
static scr_notify_layout_t scr_notify_layout;

static int SCR_BuildNotifyList(notify_draw_t *list, int max_lines, bool message_active)
{
    int stored = scr_notify_head > MAX_NOTIFY_LINES ? MAX_NOTIFY_LINES : (int)scr_notify_head;
    unsigned start = scr_notify_head - stored;
    int count = 0;

    for (int i = 0; i < stored; i++) {
        notifyline_t *line = &scr_notify_lines[(start + i) & NOTIFY_LINE_MASK];
        if (!line->time)
            continue;

        float alpha = 1.0f;
        if (!message_active) {
            alpha = SCR_FadeAlpha(line->time, NOTIFY_LIFETIME_MS, NOTIFY_FADE_MS);
            if (!alpha)
                continue;
        }

        list[count++] = notify_draw_t{ .line = line, .alpha = alpha };
        if (count == max_lines)
            break;
    }

    return count;
}

static void SCR_GetNotifySafeRect(int *out_x, int *out_y, int *out_w, int *out_h)
{
    float safe = scr_safe_zone ? scr_safe_zone->value : 0.0f;
    safe = Q_clipf(safe, 0.0f, 0.5f);

    int inset_x = Q_rint(scr.hud_width * safe);
    int inset_y = Q_rint(scr.hud_height * safe);

    inset_x = max(0, inset_x);
    inset_y = max(0, inset_y);

    int safe_w = max(0, scr.hud_width - (inset_x * 2));
    int safe_h = max(0, scr.hud_height - (inset_y * 2));

    if (out_x)
        *out_x = inset_x;
    if (out_y)
        *out_y = inset_y;
    if (out_w)
        *out_w = safe_w;
    if (out_h)
        *out_h = safe_h;
}

static int SCR_ClampNotifyPromptSkip(int prompt_skip, int max_chars)
{
    int max_prompt = max(0, max_chars - 1);
    if (prompt_skip > max_prompt)
        return max_prompt;
    if (prompt_skip < 0)
        return 0;
    return prompt_skip;
}

static void SCR_DrawNotifyChatLine(int x, int y, int flags, size_t maxlen,
                                   const char *s, color_t color)
{
    if (!scr.ui_font || !s || !*s) {
        SCR_DrawStringStretch(x, y, 1, flags, maxlen, s, color, scr.ui_font_pic);
        return;
    }

    size_t len = strlen(s);
    if (len > maxlen)
        len = maxlen;

    const char *colon = (const char *)memchr(s, ':', len);
    if (!colon || colon == s) {
        Font_DrawString(scr.ui_font, x, y, 1, flags, len, s, color);
        return;
    }

    size_t name_len = (size_t)(colon - s);
    size_t rest_len = len - name_len;

    char name_buf[MAX_STRING_CHARS];
    char rest_buf[MAX_STRING_CHARS];
    name_len = min(name_len, sizeof(name_buf) - 1);
    rest_len = min(rest_len, sizeof(rest_buf) - 1);

    memcpy(name_buf, s, name_len);
    name_buf[name_len] = '\0';
    memcpy(rest_buf, colon, rest_len);
    rest_buf[rest_len] = '\0';

    const int bold_extra = max(2, scr.hud_scale > 0.0f ? Q_rint(scr.hud_scale) : 1);
    const int name_width = Font_MeasureString(scr.ui_font, 1, flags, name_len, name_buf, nullptr);
    const int bold_flags = flags & ~UI_DROPSHADOW;

    Font_DrawString(scr.ui_font, x, y, 1, flags, name_len, name_buf, color);
    for (int i = 1; i <= bold_extra; ++i) {
        Font_DrawString(scr.ui_font, x + i, y, 1, bold_flags, name_len, name_buf, color);
    }
    Font_DrawString(scr.ui_font, x + name_width + bold_extra, y, 1, flags,
                    rest_len, rest_buf, color);
}

static void SCR_BuildNotifyLayout(scr_notify_layout_t *layout, int total_lines, int max_visible, bool message_active)
{
    int safe_x, safe_y, safe_w, safe_h;
    SCR_GetNotifySafeRect(&safe_x, &safe_y, &safe_w, &safe_h);
    int gap = CONCHAR_WIDTH / 2;
    int scrollbar_w = max(2, CONCHAR_WIDTH / 2);
    int available_width = safe_w - scrollbar_w - gap;
    int max_chars = available_width / CONCHAR_WIDTH;

    if (max_chars > 64)
        max_chars = 64;
    if (max_chars < 10)
        max_chars = max(1, max_chars);

    int width = max_chars * CONCHAR_WIDTH;
    int bottom = safe_y + safe_h - CONCHAR_HEIGHT - NOTIFY_STATUSBAR_OFFSET;
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
    if (message_active) {
        Con_GetChatPromptText(&layout->prompt_skip);
        layout->prompt_skip = SCR_ClampNotifyPromptSkip(layout->prompt_skip, layout->max_chars);
    }
}

static void SCR_SetNotifyScrollFromThumb(const scr_notify_layout_t *layout, int thumb_top)
{
    int travel = layout->scroll_track_height - layout->thumb_height;
    if (travel <= 0 || scr_notify_scroll_max <= 0.0f) {
        scr_notify_scroll_target = 0.0f;
        return;
    }

    int clamped = Q_clip(thumb_top, layout->scroll_track_top,
                         layout->scroll_track_top + travel);
    float frac = 1.0f - (float)(clamped - layout->scroll_track_top) / (float)travel;
    scr_notify_scroll_target = frac * scr_notify_scroll_max;
}

static size_t SCR_InputClampChars(const char *text, size_t max_chars)
{
    if (!text)
        return 0;
    size_t available = UTF8_CountChars(text, strlen(text));
    if (max_chars)
        available = min(available, max_chars);
    return available;
}

static size_t SCR_InputCharsForWidth(const char *text, size_t max_chars, int pixel_width)
{
    if (!text || pixel_width <= 0 || max_chars == 0)
        return 0;

    size_t available = SCR_InputClampChars(text, max_chars);
    size_t chars = 0;
    while (chars < available) {
        size_t next_bytes = UTF8_OffsetForChars(text, chars + 1);
        int width = SCR_MeasureFontString(text, next_bytes);
        if (width > pixel_width)
            break;
        chars++;
    }
    return chars;
}

static size_t SCR_InputOffsetForWidth(const char *text, size_t cursor_chars, int pixel_width)
{
    if (!text || pixel_width <= 0)
        return 0;

    size_t cursor_bytes = UTF8_OffsetForChars(text, cursor_chars);
    for (size_t start = 0; start < cursor_chars; ++start) {
        size_t start_bytes = UTF8_OffsetForChars(text, start);
        size_t len_bytes = cursor_bytes - start_bytes;
        int width = SCR_MeasureFontString(text + start_bytes, len_bytes);
        if (width <= pixel_width)
            return start;
    }
    return cursor_chars;
}

static void SCR_UpdateNotifyDrag(const scr_notify_layout_t *layout, bool message_active)
{
    if (!scr_notify_drag) {
        return;
    }

    if (!message_active || !Key_IsDown(K_MOUSE1)) {
        scr_notify_drag = false;
        return;
    }

    if (!scr_notify_mouse_valid || layout->total_lines <= layout->max_visible) {
        scr_notify_drag = false;
        return;
    }

    int thumb_top = scr_notify_mouse_y - scr_notify_drag_offset;
    SCR_SetNotifyScrollFromThumb(layout, thumb_top);
}

static void SCR_NotifySetChatCursorFromMouse(const scr_notify_layout_t *layout)
{
    inputField_t *field = Con_GetChatInputField();
    if (!field || !field->maxChars || !field->visibleChars)
        return;
    if (layout->prompt_skip >= layout->max_chars)
        return;

    const char *prompt = Con_GetChatPromptText(NULL);
    int prompt_width = SCR_MeasureFontString(prompt, layout->prompt_skip);
    int text_x = layout->input_x + prompt_width;
    int text_w = max(0, layout->width - prompt_width);

    if (scr_notify_mouse_x < text_x || scr_notify_mouse_x >= text_x + text_w)
        return;

    size_t total_chars = SCR_InputClampChars(field->text, field->maxChars);
    size_t cursor_chars = UTF8_CountChars(field->text, field->cursorPos);
    if (cursor_chars > total_chars)
        cursor_chars = total_chars;

    size_t offset_chars = SCR_InputOffsetForWidth(field->text, cursor_chars, text_w);
    if (offset_chars > total_chars)
        offset_chars = total_chars;

    size_t offset = UTF8_OffsetForChars(field->text, offset_chars);
    const char *text = field->text + offset;
    size_t remaining_chars = (offset_chars < total_chars) ? (total_chars - offset_chars) : 0;
    size_t max_chars = SCR_InputCharsForWidth(text, remaining_chars, text_w);
    int click_x = scr_notify_mouse_x - text_x;
    if (click_x < 0)
        click_x = 0;

    size_t click_bytes = 0;
    for (size_t i = 0; i < max_chars; ++i) {
        size_t len_bytes = UTF8_OffsetForChars(text, i + 1);
        int width = SCR_MeasureFontString(text, len_bytes);
        if (width > click_x)
            break;
        click_bytes = len_bytes;
    }

    size_t new_pos = offset + click_bytes;
    IF_SetCursor(field, new_pos, Key_IsDown(K_SHIFT));
}

static void SCR_DrawInputField(const inputField_t *field, int x, int y, int flags,
                               int pixel_width, size_t max_chars, color_t color)
{
    if (!field || !scr.ui_font)
        return;
    if (!field->maxChars || !field->visibleChars)
        return;

    size_t total_chars = SCR_InputClampChars(field->text, max_chars);
    size_t cursor_chars = UTF8_CountChars(field->text, field->cursorPos);
    if (cursor_chars > total_chars)
        cursor_chars = total_chars;
    size_t offset_chars = SCR_InputOffsetForWidth(field->text, cursor_chars, pixel_width);
    if (offset_chars > total_chars)
        offset_chars = total_chars;

    size_t remaining_chars = (offset_chars < total_chars) ? (total_chars - offset_chars) : 0;
    size_t draw_chars = SCR_InputCharsForWidth(field->text + UTF8_OffsetForChars(field->text, offset_chars),
                                               remaining_chars, pixel_width);

    size_t cursor_chars_visible = (cursor_chars > offset_chars) ? (cursor_chars - offset_chars) : 0;
    if (cursor_chars_visible > draw_chars)
        cursor_chars_visible = draw_chars;

    size_t offset = UTF8_OffsetForChars(field->text, offset_chars);
    const char *text = field->text + offset;
    size_t draw_len = UTF8_OffsetForChars(text, draw_chars);
    size_t cursor_bytes = UTF8_OffsetForChars(text, cursor_chars_visible);
    if (field->selecting && field->selectionAnchor != field->cursorPos) {
        size_t sel_start = min(field->selectionAnchor, field->cursorPos);
        size_t sel_end = max(field->selectionAnchor, field->cursorPos);
        size_t sel_start_chars = UTF8_CountChars(field->text, sel_start);
        size_t sel_end_chars = UTF8_CountChars(field->text, sel_end);
        size_t sel_start_visible = (sel_start_chars > offset_chars)
            ? (sel_start_chars - offset_chars)
            : 0;
        size_t sel_end_visible = (sel_end_chars > offset_chars)
            ? (sel_end_chars - offset_chars)
            : 0;
        if (sel_start_visible > draw_chars)
            sel_start_visible = draw_chars;
        if (sel_end_visible > draw_chars)
            sel_end_visible = draw_chars;

        if (sel_end_visible > sel_start_visible) {
            size_t sel_start_bytes = UTF8_OffsetForChars(text, sel_start_visible);
            size_t sel_end_bytes = UTF8_OffsetForChars(text, sel_end_visible);
            int sel_start_x = x + SCR_MeasureFontString(text, sel_start_bytes);
            int sel_end_x = x + SCR_MeasureFontString(text, sel_end_bytes);
            int sel_w = max(0, sel_end_x - sel_start_x);
            int text_h = Font_LineHeight(scr.ui_font, 1);
            color_t highlight = COLOR_RGBA(80, 120, 200, 120);
            R_DrawFill32(sel_start_x, y, sel_w, text_h, highlight);
        }
    }
    Font_DrawString(scr.ui_font, x, y, 1, flags, draw_len, text, color);

    if ((flags & UI_DRAWCURSOR) && (com_localTime & BIT(8))) {
        int cursor_x = x + SCR_MeasureFontString(text, cursor_bytes);
        size_t next_chars = min(draw_chars, cursor_chars_visible + 1);
        size_t next_bytes = UTF8_OffsetForChars(text, next_chars);
        int next_x = x + SCR_MeasureFontString(text, next_bytes);
        int cursor_w = max(0, next_x - cursor_x);
        int text_h = Font_LineHeight(scr.ui_font, 1);
        if (Key_GetOverstrikeMode()) {
            int width = max(2, cursor_w);
            if (width < 2)
                width = max(2, text_h / 2);
            color_t fill = COLOR_SETA_U8(color, 160);
            R_DrawFill32(cursor_x, y, width, text_h, fill);
        } else {
            color_t fill = COLOR_SETA_U8(color, 220);
            R_DrawFill32(cursor_x, y, 1, text_h, fill);
        }
    }
}

void SCR_ClearChatHUD_f(void)
{
    if (cgame && cgame->ChatHud_Clear) {
        cgame->ChatHud_Clear(0);
        return;
    }

    memset(scr_notify_lines, 0, sizeof(scr_notify_lines));
    scr_notify_head = 0;
    scr_notify_scroll = 0.0f;
    scr_notify_scroll_target = 0.0f;
    scr_notify_scroll_max = 0.0f;
    scr_notify_drag = false;
    scr_notify_mouse_valid = false;
    scr_notify_layout.valid = false;
}

void SCR_AddToNotifyHUD(const char *text, bool is_chat)
{
    if (cgame && cgame->ChatHud_AddLine) {
        cgame->ChatHud_AddLine(0, text, is_chat);
        return;
    }

    notifyline_t *line;
    char *p;

    line = &scr_notify_lines[scr_notify_head++ & NOTIFY_LINE_MASK];
    Q_strlcpy(line->text, text, sizeof(line->text));
    line->time = cls.realtime;
    line->is_chat = is_chat;

    p = strrchr(line->text, '\n');
    if (p)
        *p = 0;

    if ((cls.key_dest & KEY_MESSAGE) && scr_notify_scroll_target > 0.0f) {
        scr_notify_scroll_target += 1.0f;
    }
}

void SCR_AddToChatHUD(const char *text)
{
    SCR_AddToNotifyHUD(text, true);
}

void SCR_NotifyScrollLines(float delta)
{
    if (cgame && cgame->ChatHud_ScrollLines) {
        cgame->ChatHud_ScrollLines(delta);
        return;
    }

    if (!(cls.key_dest & KEY_MESSAGE))
        return;

    scr_notify_scroll_target = Q_clipf(scr_notify_scroll_target + delta, 0.0f, scr_notify_scroll_max);
}

void SCR_NotifyMouseEvent(int x, int y)
{
    if (cgame && cgame->ChatHud_MouseEvent) {
        cgame->ChatHud_MouseEvent(x, y);
        return;
    }

    if (!scr.initialized) {
        return;
    }

    if (scr.virtual_width == 0 || scr.virtual_height == 0) {
        SCR_UpdateVirtualScreen();
    }

    float scale = scr.virtual_scale > 0.0f ? (scr.hud_scale / scr.virtual_scale) : 1.0f;
    int hud_width = Q_rint(scr.virtual_width * scr.hud_scale);
    int hud_height = Q_rint(scr.virtual_height * scr.hud_scale);

    scr_notify_mouse_x = Q_clip(Q_rint(x * scale), 0, max(0, hud_width - 1));
    scr_notify_mouse_y = Q_clip(Q_rint(y * scale), 0, max(0, hud_height - 1));
    scr_notify_mouse_valid = true;
}

void SCR_NotifyMouseDown(int button)
{
    if (cgame && cgame->ChatHud_MouseDown) {
        cgame->ChatHud_MouseDown(button);
        return;
    }

    if (button != K_MOUSE1)
        return;
    if (!(cls.key_dest & KEY_MESSAGE))
        return;
    if (!scr_notify_layout.valid)
        return;
    if (!scr_notify_mouse_valid)
        return;

    Con_GetChatPromptText(&scr_notify_layout.prompt_skip);
    scr_notify_layout.prompt_skip = SCR_ClampNotifyPromptSkip(scr_notify_layout.prompt_skip,
                                                              scr_notify_layout.max_chars);

    if (scr_notify_layout.total_lines > scr_notify_layout.max_visible) {
        int x = scr_notify_mouse_x;
        int y = scr_notify_mouse_y;
        int track_left = scr_notify_layout.scrollbar_x;
        int track_right = scr_notify_layout.scrollbar_x + scr_notify_layout.scrollbar_w;
        int track_top = scr_notify_layout.scroll_track_top;
        int track_bottom = track_top + scr_notify_layout.scroll_track_height;

        if (x >= track_left && x <= track_right && y >= track_top && y <= track_bottom) {
            int thumb_top = scr_notify_layout.thumb_top;
            int thumb_bottom = thumb_top + scr_notify_layout.thumb_height;

            if (y >= thumb_top && y <= thumb_bottom) {
                scr_notify_drag = true;
                scr_notify_drag_offset = y - thumb_top;
            } else {
                int target_top = y - (scr_notify_layout.thumb_height / 2);
                SCR_SetNotifyScrollFromThumb(&scr_notify_layout, target_top);
                scr_notify_drag = true;
                scr_notify_drag_offset = scr_notify_layout.thumb_height / 2;
            }
            return;
        }
    }

    SCR_NotifySetChatCursorFromMouse(&scr_notify_layout);
}

static void SCR_DrawChatHUD(color_t base_color)
{
    notify_draw_t draw_lines[MAX_NOTIFY_LINES];
    bool message_active = (cls.key_dest & KEY_MESSAGE) != 0;
    int max_visible = message_active ? NOTIFY_VISIBLE_LINES_ACTIVE : NOTIFY_VISIBLE_LINES;
    int total = SCR_BuildNotifyList(draw_lines, MAX_NOTIFY_LINES, message_active);

    if (cls.key_dest & KEY_CONSOLE)
        return;

    if (!message_active) {
        scr_notify_scroll = 0.0f;
        scr_notify_scroll_target = 0.0f;
        scr_notify_scroll_max = 0.0f;
        scr_notify_drag = false;
    }

    if (scr_chathud->integer == 0 && !message_active)
        return;

    SCR_BuildNotifyLayout(&scr_notify_layout, total, max_visible, message_active);

    scr_notify_scroll_max = max(0.0f, (float)(total - max_visible));
    if (scr_notify_scroll_max <= 0.0f) {
        scr_notify_scroll = 0.0f;
        scr_notify_scroll_target = 0.0f;
    } else {
        scr_notify_scroll = Q_clipf(scr_notify_scroll, 0.0f, scr_notify_scroll_max);
        scr_notify_scroll_target = Q_clipf(scr_notify_scroll_target, 0.0f, scr_notify_scroll_max);
    }

    if (message_active) {
        SCR_UpdateNotifyDrag(&scr_notify_layout, message_active);
        CL_AdvanceValue(&scr_notify_scroll, scr_notify_scroll_target, NOTIFY_SCROLL_SPEED);
    }

    if (total > 0) {
        float scroll_pixels = scr_notify_scroll * CONCHAR_HEIGHT;
        float y_base = scr_notify_layout.viewport_bottom + scroll_pixels;

        for (int i = 0; i < total; i++) {
            notifyline_t *line = draw_lines[i].line;
            float y = y_base - (float)(total - 1 - i) * CONCHAR_HEIGHT;

            if (y < scr_notify_layout.viewport_top - CONCHAR_HEIGHT ||
                y > scr_notify_layout.viewport_bottom) {
                continue;
            }

            color_t color = base_color;
            color.a = Q_rint((float)color.a * draw_lines[i].alpha);

            int flags = 0;
            if (scr_chathud->integer == 2 && line->is_chat)
                flags |= UI_ALTCOLOR;

            if (line->is_chat)
                SCR_DrawNotifyChatLine(scr_notify_layout.x, Q_rint(y), flags,
                                       scr_notify_layout.max_chars, line->text, color);
            else
                SCR_DrawStringStretch(scr_notify_layout.x, Q_rint(y), 1, flags,
                                      scr_notify_layout.max_chars, line->text, color,
                                      scr.ui_font_pic);
        }
    }

    if (message_active) {
        int prompt_skip = 0;
        const char *prompt = Con_GetChatPromptText(&prompt_skip);
        prompt_skip = SCR_ClampNotifyPromptSkip(prompt_skip, scr_notify_layout.max_chars);
        scr_notify_layout.prompt_skip = prompt_skip;
        int prompt_width = SCR_MeasureFontString(prompt, prompt_skip);

        int visible_chars = scr_notify_layout.max_chars - prompt_skip;
        if (visible_chars < 1)
            visible_chars = 1;

        inputField_t *field = Con_GetChatInputField();
        if (field) {
            field->visibleChars = min((size_t)visible_chars, field->maxChars);
        }

        SCR_DrawStringStretch(scr_notify_layout.input_x, scr_notify_layout.input_y, 1, 0,
                              scr_notify_layout.max_chars, prompt, base_color, scr.ui_font_pic);
        if (field) {
            int text_w = max(0, scr_notify_layout.width - prompt_width);
            SCR_DrawInputField(field, scr_notify_layout.input_x + prompt_width,
                               scr_notify_layout.input_y, UI_DRAWCURSOR, text_w,
                               scr_notify_layout.max_chars - prompt_skip, COLOR_WHITE);
        }
    }

    if (message_active && total > max_visible) {
        float frac = scr_notify_scroll_max > 0.0f ? (scr_notify_scroll / scr_notify_scroll_max) : 0.0f;
        int track_height = scr_notify_layout.scroll_track_height;
        int thumb_height = max(6, Q_rint(track_height * ((float)max_visible / (float)total)));
        int travel = track_height - thumb_height;
        int thumb_top = scr_notify_layout.scroll_track_top + Q_rint((1.0f - frac) * travel);

        scr_notify_layout.thumb_top = thumb_top;
        scr_notify_layout.thumb_height = thumb_height;

        color_t track = COLOR_RGBA(0, 0, 0, Q_rint(base_color.a * 0.35f));
        color_t thumb = COLOR_RGBA(255, 255, 255, Q_rint(base_color.a * 0.75f));

        R_DrawFill32(scr_notify_layout.scrollbar_x, scr_notify_layout.scroll_track_top,
                     scr_notify_layout.scrollbar_w, track_height, track);
        R_DrawFill32(scr_notify_layout.scrollbar_x, thumb_top,
                     scr_notify_layout.scrollbar_w, thumb_height, thumb);
    }
}

/*
===============================================================================

DEBUG STUFF

===============================================================================
*/

static void SCR_DrawTurtle(color_t base_color)
{
    int x, y;

    if (scr_showturtle->integer <= 0)
        return;

    if (!cl.frameflags)
        return;

    x = CONCHAR_WIDTH;
    y = scr.hud_height - 11 * CONCHAR_HEIGHT;

#define DF(f) \
    if (cl.frameflags & FF_##f) { \
        SCR_DrawString(x, y, UI_ALTCOLOR, base_color, #f); \
        y += CONCHAR_HEIGHT; \
    }

    if (scr_showturtle->integer > 1) {
        DF(SUPPRESSED)
    }
    DF(CLIENTPRED)
    if (scr_showturtle->integer > 1) {
        DF(CLIENTDROP)
        DF(SERVERDROP)
    }
    DF(BADFRAME)
    DF(OLDFRAME)
    DF(OLDENT)
    DF(NODELTA)

#undef DF
}

#if USE_DEBUG

static void SCR_DrawDebugStats(void)
{
    char buffer[MAX_QPATH];
    int i, j;
    int x, y;

    j = scr_showstats->integer;
    if (j <= 0)
        return;

    if (j > cl.max_stats)
        j = cl.max_stats;

    x = CONCHAR_WIDTH;
    y = (scr.hud_height - j * CONCHAR_HEIGHT) / 2;
    for (i = 0; i < j; i++) {
        Q_snprintf(buffer, sizeof(buffer), "%2d: %d", i, cl.frame.ps.stats[i]);
        color_t color = COLOR_WHITE;
        if (cl.oldframe.ps.stats[i] != cl.frame.ps.stats[i]) {
            color = COLOR_RED;
        }
        SCR_DrawString(x, y, 0, color, buffer);
        y += CONCHAR_HEIGHT;
    }
}

static void SCR_DrawDebugPmove(void)
{
    static const char * const types[] = {
        "NORMAL", "SPECTATOR", "DEAD", "GIB", "FREEZE"
    };
    static const char * const flags[] = {
        "DUCKED", "JUMP_HELD", "ON_GROUND",
        "TIME_WATERJUMP", "TIME_LAND", "TIME_TELEPORT",
        "NO_PREDICTION", "TELEPORT_BIT"
    };
    unsigned i, j;
    int x, y;

    if (!scr_showpmove->integer)
        return;

    x = CONCHAR_WIDTH;
    y = (scr.hud_height - 2 * CONCHAR_HEIGHT) / 2;

    i = cl.frame.ps.pmove.pm_type;
    if (i > PM_FREEZE)
        i = PM_FREEZE;

    SCR_DrawString(x, y, 0, COLOR_WHITE, types[i]);
    y += CONCHAR_HEIGHT;

    j = cl.frame.ps.pmove.pm_flags;
    for (i = 0; i < 8; i++) {
        if (j & (1 << i)) {
            x = SCR_DrawString(x, y, 0, COLOR_WHITE, flags[i]);
            x += CONCHAR_WIDTH;
        }
    }
}

#endif

//============================================================================

// Sets scr_vrect, the coordinates of the rendered window
static void SCR_CalcVrect(void)
{
    int     size;

    // bound viewsize
    size = Cvar_ClampInteger(scr_viewsize, 40, 100);

    scr.vrect.width = scr.canvas_width * size / 100;
    scr.vrect.height = scr.canvas_height * size / 100;

    scr.vrect.x = (scr.canvas_width - scr.vrect.width) / 2;
    scr.vrect.y = (scr.canvas_height - scr.vrect.height) / 2;
}

/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
static void SCR_SizeUp_f(void)
{
    Cvar_SetInteger(scr_viewsize, scr_viewsize->integer + 10, FROM_CONSOLE);
}

/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
static void SCR_SizeDown_f(void)
{
    Cvar_SetInteger(scr_viewsize, scr_viewsize->integer - 10, FROM_CONSOLE);
}

/*
=================
SCR_Sky_f

Set a specific sky and rotation speed. If empty sky name is provided, falls
back to server defaults.
=================
*/
static void SCR_Sky_f(void)
{
    char    *name;
    float   rotate;
    vec3_t  axis;
    int     argc = Cmd_Argc();

    if (argc < 2) {
        Com_Printf("Usage: sky <basename> [rotate] [axis x y z]\n");
        return;
    }

    if (cls.state != ca_active) {
        Com_Printf("No map loaded.\n");
        return;
    }

    name = Cmd_Argv(1);
    if (!*name) {
        CL_SetSky();
        return;
    }

    if (argc > 2)
        rotate = Q_atof(Cmd_Argv(2));
    else
        rotate = 0;

    if (argc == 6) {
        axis[0] = Q_atof(Cmd_Argv(3));
        axis[1] = Q_atof(Cmd_Argv(4));
        axis[2] = Q_atof(Cmd_Argv(5));
    } else
        VectorSet(axis, 0, 0, 1);
    
    R_SetSky(name, rotate, true, axis);
}

/*
================
SCR_TimeRenderer_f
================
*/
static void SCR_TimeRenderer_f(void)
{
    int     i;
    unsigned    start, stop;
    float       time;

    if (cls.state != ca_active) {
        Com_Printf("No map loaded.\n");
        return;
    }

    start = Sys_Milliseconds();

    cl.refdef.frametime = 0.0f;
    if (Cmd_Argc() == 2) {
        // run without page flipping
        R_BeginFrame();
        for (i = 0; i < 128; i++) {
            cl.refdef.viewangles[1] = i / 128.0f * 360.0f;
            R_RenderFrame(&cl.refdef);
        }
        R_EndFrame();
    } else {
        for (i = 0; i < 128; i++) {
            cl.refdef.viewangles[1] = i / 128.0f * 360.0f;

            R_BeginFrame();
            R_RenderFrame(&cl.refdef);
            R_EndFrame();
        }
    }

    stop = Sys_Milliseconds();
    time = (stop - start) * 0.001f;
    Com_Printf("%f seconds (%f fps)\n", time, 128.0f / time);
}


//============================================================================

static void cl_crosshair_size_changed(cvar_t *self)
{
    int w, h;
    float hit_marker_scale = 1.0f;

    (void)self;

    if (cl_crosshair_size) {
        float size = Cvar_ClampValue(cl_crosshair_size, 1.0f, 512.0f);
        hit_marker_scale = Q_clipf(size / 32.0f, 0.1f, 9.0f);
    }

    R_GetPicSize(&w, &h, scr.hit_marker_pic);
    scr.hit_marker_width = Q_rint(w * hit_marker_scale);
    scr.hit_marker_height = Q_rint(h * hit_marker_scale);
}

typedef struct {
    cvar_t **primary;
    cvar_t **legacy;
    xchanged_t changed;
} cvar_alias_pair_t;

static bool cl_screen_alias_syncing;

static cvar_alias_pair_t cl_screen_aliases[] = {
    { &scr_showpause, &scr_showpause_legacy, NULL },
#if USE_DEBUG
    { &scr_showstats, &scr_showstats_legacy, NULL },
    { &scr_showpmove, &scr_showpmove_legacy, NULL },
#endif
    { &scr_showturtle, &scr_showturtle_legacy, NULL },
    { &scr_demobar, &scr_demobar_legacy, NULL },
    { &scr_font, &scr_font_legacy, scr_font_changed },
    { &scr_scale, &scr_scale_legacy, scr_scale_changed },
    { &scr_chathud, &scr_chathud_legacy, NULL },
    { &scr_chathud_lines, &scr_chathud_lines_legacy, NULL },
    { &scr_chathud_time, &scr_chathud_time_legacy, cl_timeout_changed },
    { &scr_chathud_x, &scr_chathud_x_legacy, NULL },
    { &scr_chathud_y, &scr_chathud_y_legacy, NULL },
    { &scr_draw2d, &scr_draw2d_legacy, NULL },
    { &scr_lag_x, &scr_lag_x_legacy, NULL },
    { &scr_lag_y, &scr_lag_y_legacy, NULL },
    { &scr_lag_draw, &scr_lag_draw_legacy, NULL },
    { &scr_lag_min, &scr_lag_min_legacy, NULL },
    { &scr_lag_max, &scr_lag_max_legacy, NULL },
    { &scr_alpha, &scr_alpha_legacy, NULL },
    { &scr_hit_marker_time, &scr_hit_marker_time_legacy, NULL },
    { &scr_damage_indicators, &scr_damage_indicators_legacy, NULL },
    { &scr_damage_indicator_time, &scr_damage_indicator_time_legacy, NULL },
    { &scr_pois, &scr_pois_legacy, NULL },
    { &scr_poi_edge_frac, &scr_poi_edge_frac_legacy, NULL },
    { &scr_poi_max_scale, &scr_poi_max_scale_legacy, NULL },
    { &scr_safe_zone, &scr_safe_zone_legacy, NULL },
};

static void cl_screen_alias_changed(cvar_t *self)
{
    if (cl_screen_alias_syncing)
        return;

    cl_screen_alias_syncing = true;

    for (size_t i = 0; i < q_countof(cl_screen_aliases); i++) {
        cvar_alias_pair_t *pair = &cl_screen_aliases[i];
        cvar_t *primary = *pair->primary;
        cvar_t *legacy = *pair->legacy;

        if (!primary || !legacy)
            continue;

        if (self == primary) {
            Cvar_SetByVar(legacy, primary->string, FROM_CODE);
            if (pair->changed)
                pair->changed(primary);
            break;
        }

        if (self == legacy) {
            Cvar_SetByVar(primary, legacy->string, FROM_CODE);
            if (pair->changed)
                pair->changed(primary);
            break;
        }
    }

    cl_screen_alias_syncing = false;
}

static void cl_screen_alias_sync_defaults(void)
{
    if (cl_screen_alias_syncing)
        return;

    cl_screen_alias_syncing = true;

    for (size_t i = 0; i < q_countof(cl_screen_aliases); i++) {
        cvar_alias_pair_t *pair = &cl_screen_aliases[i];
        cvar_t *primary = *pair->primary;
        cvar_t *legacy = *pair->legacy;

        if (!primary || !legacy)
            continue;

        if (!(primary->flags & CVAR_MODIFIED) && (legacy->flags & CVAR_MODIFIED))
            Cvar_SetByVar(primary, legacy->string, FROM_CODE);
        else
            Cvar_SetByVar(legacy, primary->string, FROM_CODE);
    }

    cl_screen_alias_syncing = false;
}

static void cl_screen_alias_register(void)
{
    for (size_t i = 0; i < q_countof(cl_screen_aliases); i++) {
        cvar_alias_pair_t *pair = &cl_screen_aliases[i];
        if (*pair->primary)
            (*pair->primary)->changed = cl_screen_alias_changed;
        if (*pair->legacy)
            (*pair->legacy)->changed = cl_screen_alias_changed;
    }

    cl_screen_alias_sync_defaults();
}

static bool cl_crosshair_alias_syncing;

static cvar_alias_pair_t cl_crosshair_aliases[] = {
    { &cl_crosshair_brightness, &cl_crosshair_brightness_legacy, NULL },
    { &cl_crosshair_color, &cl_crosshair_color_legacy, NULL },
    { &cl_crosshair_health, &cl_crosshair_health_legacy, NULL },
    { &cl_crosshair_hit_color, &cl_crosshair_hit_color_legacy, NULL },
    { &cl_crosshair_hit_style, &cl_crosshair_hit_style_legacy, NULL },
    { &cl_crosshair_hit_time, &cl_crosshair_hit_time_legacy, NULL },
    { &cl_crosshair_pulse, &cl_crosshair_pulse_legacy, NULL },
    { &cl_crosshair_size, &cl_crosshair_size_legacy, cl_crosshair_size_changed },
};

static void cl_crosshair_alias_changed(cvar_t *self)
{
    if (cl_crosshair_alias_syncing)
        return;

    cl_crosshair_alias_syncing = true;

    for (size_t i = 0; i < q_countof(cl_crosshair_aliases); i++) {
        cvar_alias_pair_t *pair = &cl_crosshair_aliases[i];
        cvar_t *primary = *pair->primary;
        cvar_t *legacy = *pair->legacy;

        if (!primary || !legacy)
            continue;

        if (self == primary) {
            Cvar_SetByVar(legacy, primary->string, FROM_CODE);
            if (pair->changed)
                pair->changed(primary);
            break;
        }

        if (self == legacy) {
            Cvar_SetByVar(primary, legacy->string, FROM_CODE);
            if (pair->changed)
                pair->changed(primary);
            break;
        }
    }

    cl_crosshair_alias_syncing = false;
}

static void cl_crosshair_alias_sync_defaults(void)
{
    if (cl_crosshair_alias_syncing)
        return;

    cl_crosshair_alias_syncing = true;

    for (size_t i = 0; i < q_countof(cl_crosshair_aliases); i++) {
        cvar_alias_pair_t *pair = &cl_crosshair_aliases[i];
        cvar_t *primary = *pair->primary;
        cvar_t *legacy = *pair->legacy;

        if (!primary || !legacy)
            continue;

        if (!(primary->flags & CVAR_MODIFIED) && (legacy->flags & CVAR_MODIFIED))
            Cvar_SetByVar(primary, legacy->string, FROM_CODE);
        else
            Cvar_SetByVar(legacy, primary->string, FROM_CODE);
    }

    cl_crosshair_alias_syncing = false;
}

static void cl_crosshair_alias_register(void)
{
    for (size_t i = 0; i < q_countof(cl_crosshair_aliases); i++) {
        cvar_alias_pair_t *pair = &cl_crosshair_aliases[i];
        if (*pair->primary)
            (*pair->primary)->changed = cl_crosshair_alias_changed;
        if (*pair->legacy)
            (*pair->legacy)->changed = cl_crosshair_alias_changed;
    }

    cl_crosshair_alias_sync_defaults();
}

static void scr_crosshair_changed(cvar_t *self)
{
    if (self->integer > 0) {
        scr.crosshair_pic = R_RegisterPic(va("ch%i", self->integer));
        cl_crosshair_size_changed(cl_crosshair_size);
    } else {
        scr.crosshair_pic = 0;
    }
}

void SCR_SetCrosshairColor(void)
{
    color_t color = COLOR_RGBA(255, 255, 255, 255);

    if (cl_crosshair_health && cl_crosshair_health->integer) {
        int health = cl.frame.ps.stats[STAT_HEALTH];
        if (health <= 0) {
            color = COLOR_RGBA(0, 0, 0, 255);
        } else {
            color.r = 255;

            if (health >= 66) {
                color.g = 255;
            } else if (health < 33) {
                color.g = 0;
            } else {
                color.g = (255 * (health - 33)) / 33;
            }

            if (health >= 99) {
                color.b = 255;
            } else if (health < 66) {
                color.b = 0;
            } else {
                color.b = (255 * (health - 66)) / 33;
            }
        }
    } else if (cl_crosshair_color) {
        int index = Cvar_ClampInteger(cl_crosshair_color, 1, (int)q_countof(ql_crosshair_colors));
        color = SCR_GetCrosshairPaletteColor(index);
    }

    color = SCR_ApplyCrosshairBrightness(color);
    color.a = 255;
    scr.crosshair_color = color;
}

void SCR_ModeChanged(void)
{
    IN_Activate();
    SCR_UpdateVirtualScreen();
    scr.canvas_width = r_config.width;
    scr.canvas_height = r_config.height;
    Con_CheckResize();
    if (cls.ref_initialized)
        UI_FontModeChanged();
    UI_ModeChanged();
    cls.disable_screen = 0;
    if (scr.initialized) {
        scr.hud_scale = R_ClampScale(scr_scale);
        scr_font_changed(scr_font);
    }
}

static void scr_font_changed(cvar_t *self)
{
    if (scr.ui_font == scr.font) {
        scr.ui_font = nullptr;
        scr.ui_font_pic = 0;
    }
    if (scr.font) {
        Font_Free(scr.font);
        scr.font = nullptr;
    }

    float pixel_scale = SCR_GetFontPixelScale();
    scr.font = Font_Load(self->string, CONCHAR_HEIGHT, pixel_scale, 0,
                         "fonts/qfont.kfont", "conchars.png");

    if (!scr.font && strcmp(self->string, self->default_string)) {
        Cvar_Reset(self);
        scr.font = Font_Load(self->default_string, CONCHAR_HEIGHT, pixel_scale, 0,
                             "fonts/qfont.kfont", "conchars.png");
    }

    Font_SetLetterSpacing(scr.font, k_scr_ttf_letter_spacing);
    scr.font_pic = Font_LegacyHandle(scr.font);

    scr_ui_font_reload();
}

static void scr_ui_font_reload(void)
{
    if (scr.ui_font && scr.ui_font != scr.font) {
        Font_Free(scr.ui_font);
        scr.ui_font = nullptr;
    }
    if (scr.ui_font == scr.font)
        scr.ui_font = nullptr;

    float pixel_scale = SCR_GetFontPixelScale();
    scr.ui_font = Font_Load(k_scr_ui_font_path, CONCHAR_HEIGHT, pixel_scale, 0,
                            "fonts/qfont.kfont", "conchars.png");
    if (!scr.ui_font) {
        scr.ui_font = scr.font;
        scr.ui_font_pic = scr.font_pic;
        return;
    }

    Font_SetLetterSpacing(scr.ui_font, k_scr_ttf_letter_spacing);
    scr.ui_font_pic = Font_LegacyHandle(scr.ui_font);
}

/*
==================
SCR_Clear
==================
*/
void SCR_Clear(void)
{
    memset(scr.damage_entries, 0, sizeof(scr.damage_entries));
    memset(scr.pois, 0, sizeof(scr.pois));
    scr_crosshair_pulse_time = 0;
    scr_last_pickup_icon = 0;
    scr_last_pickup_string = 0;
}

/*
==================
SCR_RegisterMedia
==================
*/
void SCR_RegisterMedia(void)
{
    int     i;

    for (i = 0; i < STAT_MINUS; i++)
        scr.sb_pics[0][i] = R_RegisterPic(va("num_%d", i));
    scr.sb_pics[0][i] = R_RegisterPic("num_minus");

    for (i = 0; i < STAT_MINUS; i++)
        scr.sb_pics[1][i] = R_RegisterPic(va("anum_%d", i));
    scr.sb_pics[1][i] = R_RegisterPic("anum_minus");

    scr.inven_pic = R_RegisterPic("inventory");
    scr.field_pic = R_RegisterPic("field_3");
    scr.backtile_pic = R_RegisterImage("backtile", IT_PIC,
                                       static_cast<imageflags_t>(IF_PERMANENT | IF_REPEAT));
    scr.pause_pic = R_RegisterPic("pause");
    scr.loading_pic = R_RegisterPic("loading");

    scr.damage_display_pic = R_RegisterPic("damage_indicator");
    R_GetPicSize(&scr.damage_display_width, &scr.damage_display_height, scr.damage_display_pic);

    scr.net_pic = R_RegisterPic("net");
    scr.hit_marker_pic = R_RegisterImage("marker", IT_PIC,
                                         static_cast<imageflags_t>(IF_PERMANENT | IF_OPTIONAL));

    scr_crosshair_changed(scr_crosshair);

    if (cgame)
        cgame->TouchPics();

    SCR_LoadKFont(&scr.kfont, "fonts/qconfont.kfont");

    scr_font_changed(scr_font);
}

static void scr_scale_changed(cvar_t *self)
{
    scr.hud_scale = R_ClampScale(self);
    if (scr.initialized && scr_font)
        scr_font_changed(scr_font);
}


//============================================================================

typedef struct stat_reg_s stat_reg_t;

typedef struct stat_reg_s {
    char        name[MAX_QPATH];
    xcommand_t  cb;

    stat_reg_t  *next;
} stat_reg_t;

static const stat_reg_t *stat_active;
static stat_reg_t *stat_head;

struct {
    int x, y;
    int key_width, value_width;
    int key_id;
} stat_state;

void SCR_RegisterStat(const char *name, xcommand_t cb)
{
    stat_reg_t *reg = static_cast<stat_reg_t *>(Z_TagMalloc(sizeof(stat_reg_t), TAG_CMD));
    reg->next = stat_head;
    Q_strlcpy(reg->name, name, sizeof(reg->name));
    reg->cb = cb;
    stat_head = reg;
}

void SCR_UnregisterStat(const char *name)
{
    stat_reg_t **rover = &stat_head;

    while (*rover) {
        if (!strcmp((*rover)->name, name)) {
            stat_reg_t *s = *rover;
            *rover = s->next;

            if (stat_active == s)
                stat_active = NULL;

            Z_Free(s);
            return;
        }

        rover = &(*rover)->next;
    }

    Com_EPrintf("can't unregister missing stat \"%s\"\n", name);
}

static void SCR_ToggleStat(const char *name)
{
    stat_reg_t *rover = stat_head;

    while (rover) {
        if (!strcmp(rover->name, name)) {
            if (stat_active == rover) {
                stat_active = NULL;
            } else {
                stat_active = rover;
            }
            return;
        }

        rover = rover->next;
    }
}

void SCR_StatTableSize(int key_width, int value_width)
{
    stat_state.key_width = key_width;
    stat_state.value_width = value_width;
}

#define STAT_MARGIN 1

void SCR_StatKeyValue(const char *key, const char *value)
{
    int c = (stat_state.key_id & 1) ? 24 : 0;
    R_DrawFill32(stat_state.x, stat_state.y, CONCHAR_WIDTH * (stat_state.key_width + stat_state.value_width) + (STAT_MARGIN * 2), CONCHAR_HEIGHT + (STAT_MARGIN * 2), COLOR_RGBA(c, c, c, 127));
    SCR_DrawString(stat_state.x + STAT_MARGIN, stat_state.y + STAT_MARGIN, UI_DROPSHADOW, COLOR_WHITE, key);
    stat_state.x += CONCHAR_WIDTH * stat_state.key_width;
    SCR_DrawString(stat_state.x + STAT_MARGIN, stat_state.y + STAT_MARGIN, UI_DROPSHADOW, COLOR_WHITE, value);

    stat_state.x = 24;
    stat_state.y += CONCHAR_HEIGHT + (STAT_MARGIN * 2);
    stat_state.key_id++;
}

void SCR_DrawStats(void)
{
    if (!stat_active)
        return;

    stat_state.x = 24;
    stat_state.y = 24;
    stat_state.key_id = 0;

    SCR_StatTableSize(24, 32);

    stat_active->cb();
}

bool SCR_StatActive(void)
{
    return !!stat_active;
}

static void SCR_Stat_g(genctx_t *ctx)
{
    if (!stat_head)
        return;

    for (stat_reg_t *stat = stat_head; stat; stat = stat->next) {
        Prompt_AddMatch(ctx, stat->name);
    }
}

static void SCR_Stat_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        SCR_Stat_g(ctx);
    }
}

static void SCR_Stat_f(void)
{
    if (Cmd_Argc() == 2) {
        SCR_ToggleStat(Cmd_Argv(1));
    } else {
        Com_Printf("Available stats:\n");

        stat_reg_t *rover = stat_head;

        while (rover) {
            Com_Printf(" * %s\n", rover->name);
            rover = rover->next;
        }
    }
}

static const cmdreg_t scr_cmds[] = {
    { "timerenderer", SCR_TimeRenderer_f },
    { "sizeup", SCR_SizeUp_f },
    { "sizedown", SCR_SizeDown_f },
    { "sky", SCR_Sky_f },
    { "draw", SCR_Draw_f, SCR_Draw_c },
    { "undraw", SCR_UnDraw_f, SCR_UnDraw_c },
    { "clearchathud", SCR_ClearChatHUD_f },
    { "stat", SCR_Stat_f, SCR_Stat_c },
    { NULL }
};

/*
==================
SCR_Init
==================
*/
void SCR_Init(void)
{
    scr_viewsize = Cvar_Get("viewsize", "100", CVAR_ARCHIVE);
    scr_showpause = Cvar_Get("cl_showpause", "1", 0);
    scr_showpause_legacy = Cvar_Get("scr_showpause", scr_showpause->string, CVAR_NOARCHIVE);
    scr_demobar = Cvar_Get("cl_demobar", "1", 0);
    scr_demobar_legacy = Cvar_Get("scr_demobar", scr_demobar->string, CVAR_NOARCHIVE);
    scr_font = Cvar_Get("cl_font", "fonts/RussoOne-Regular.ttf", 0);
    scr_font_legacy = Cvar_Get("scr_font", scr_font->string, CVAR_NOARCHIVE);
    scr_font->changed = scr_font_changed;
    scr_scale = Cvar_Get("cl_scale", "0", 0);
    scr_scale_legacy = Cvar_Get("scr_scale", scr_scale->string, CVAR_NOARCHIVE);
    scr_scale->changed = scr_scale_changed;
    cl_font_skip_virtual_scale = Cvar_Get("cl_font_skip_virtual_scale", "0", CVAR_ARCHIVE);
    cl_font_skip_virtual_scale->changed = cl_font_skip_virtual_scale_changed;
    scr_crosshair = Cvar_Get("crosshair", "3", CVAR_ARCHIVE);
    scr_crosshair->changed = scr_crosshair_changed;

    scr_netgraph = Cvar_Get("netgraph", "0", 0);
    scr_timegraph = Cvar_Get("timegraph", "0", 0);
    scr_debuggraph = Cvar_Get("debuggraph", "0", 0);
    scr_graphheight = Cvar_Get("graphheight", "32", 0);
    scr_graphscale = Cvar_Get("graphscale", "1", 0);
    scr_graphshift = Cvar_Get("graphshift", "0", 0);

    scr_chathud = Cvar_Get("cl_chathud", "1", 0);
    scr_chathud_legacy = Cvar_Get("scr_chathud", scr_chathud->string, CVAR_NOARCHIVE);
    scr_chathud_lines = Cvar_Get("cl_chathud_lines", "4", 0);
    scr_chathud_lines_legacy = Cvar_Get("scr_chathud_lines", scr_chathud_lines->string, CVAR_NOARCHIVE);
    scr_chathud_time = Cvar_Get("cl_chathud_time", "0", 0);
    scr_chathud_time_legacy = Cvar_Get("scr_chathud_time", scr_chathud_time->string, CVAR_NOARCHIVE);
    scr_chathud_time->changed = cl_timeout_changed;
    scr_chathud_x = Cvar_Get("cl_chathud_x", "8", 0);
    scr_chathud_x_legacy = Cvar_Get("scr_chathud_x", scr_chathud_x->string, CVAR_NOARCHIVE);
    scr_chathud_y = Cvar_Get("cl_chathud_y", "-64", 0);
    scr_chathud_y_legacy = Cvar_Get("scr_chathud_y", scr_chathud_y->string, CVAR_NOARCHIVE);

    cl_crosshair_brightness = Cvar_Get("cl_crosshair_brightness", "1.0", CVAR_ARCHIVE);
    cl_crosshair_brightness_legacy = Cvar_Get("cl_crosshairBrightness", cl_crosshair_brightness->string,
                                              CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_color = Cvar_Get("cl_crosshair_color", "25", CVAR_ARCHIVE);
    cl_crosshair_color_legacy = Cvar_Get("cl_crosshairColor", cl_crosshair_color->string,
                                         CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_health = Cvar_Get("cl_crosshair_health", "0", CVAR_ARCHIVE);
    cl_crosshair_health_legacy = Cvar_Get("cl_crosshairHealth", cl_crosshair_health->string,
                                          CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_hit_color = Cvar_Get("cl_crosshair_hit_color", "1", CVAR_ARCHIVE);
    cl_crosshair_hit_color_legacy = Cvar_Get("cl_crosshairHitColor", cl_crosshair_hit_color->string,
                                             CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_hit_style = Cvar_Get("cl_crosshair_hit_style", "2", CVAR_ARCHIVE);
    cl_crosshair_hit_style_legacy = Cvar_Get("cl_crosshairHitStyle", cl_crosshair_hit_style->string,
                                             CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_hit_time = Cvar_Get("cl_crosshair_hit_time", "200", CVAR_ARCHIVE);
    cl_crosshair_hit_time_legacy = Cvar_Get("cl_crosshairHitTime", cl_crosshair_hit_time->string,
                                            CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_pulse = Cvar_Get("cl_crosshair_pulse", "1", CVAR_ARCHIVE);
    cl_crosshair_pulse_legacy = Cvar_Get("cl_crosshairPulse", cl_crosshair_pulse->string,
                                         CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_size = Cvar_Get("cl_crosshair_size", "32", CVAR_ARCHIVE);
    cl_crosshair_size_legacy = Cvar_Get("cl_crosshairSize", cl_crosshair_size->string,
                                        CVAR_ARCHIVE | CVAR_NOARCHIVE);
    cl_crosshair_alias_register();
    ch_x = Cvar_Get("ch_x", "0", 0);
    ch_y = Cvar_Get("ch_y", "0", 0);

    scr_draw2d = Cvar_Get("cl_draw2d", "2", 0);
    scr_draw2d_legacy = Cvar_Get("scr_draw2d", scr_draw2d->string, CVAR_NOARCHIVE);
    scr_showturtle = Cvar_Get("cl_showturtle", "1", 0);
    scr_showturtle_legacy = Cvar_Get("scr_showturtle", scr_showturtle->string, CVAR_NOARCHIVE);
    scr_lag_x = Cvar_Get("cl_lag_x", "-1", 0);
    scr_lag_x_legacy = Cvar_Get("scr_lag_x", scr_lag_x->string, CVAR_NOARCHIVE);
    scr_lag_y = Cvar_Get("cl_lag_y", "-1", 0);
    scr_lag_y_legacy = Cvar_Get("scr_lag_y", scr_lag_y->string, CVAR_NOARCHIVE);
    scr_lag_draw = Cvar_Get("cl_lag_draw", "0", 0);
    scr_lag_draw_legacy = Cvar_Get("scr_lag_draw", scr_lag_draw->string, CVAR_NOARCHIVE);
    scr_lag_min = Cvar_Get("cl_lag_min", "0", 0);
    scr_lag_min_legacy = Cvar_Get("scr_lag_min", scr_lag_min->string, CVAR_NOARCHIVE);
    scr_lag_max = Cvar_Get("cl_lag_max", "200", 0);
    scr_lag_max_legacy = Cvar_Get("scr_lag_max", scr_lag_max->string, CVAR_NOARCHIVE);
    cg_lagometer = Cvar_Get("cg_lagometer", "1", CVAR_ARCHIVE);
    scr_alpha = Cvar_Get("cl_alpha", "1", 0);
    scr_alpha_legacy = Cvar_Get("scr_alpha", scr_alpha->string, CVAR_NOARCHIVE);
#if USE_DEBUG
    scr_showstats = Cvar_Get("cl_showstats", "0", 0);
    scr_showstats_legacy = Cvar_Get("scr_showstats", scr_showstats->string, CVAR_NOARCHIVE);
    scr_showpmove = Cvar_Get("cl_showpmove", "0", 0);
    scr_showpmove_legacy = Cvar_Get("scr_showpmove", scr_showpmove->string, CVAR_NOARCHIVE);
#endif

    scr_hit_marker_time = Cvar_Get("cl_hit_marker_time", "500", 0);
    scr_hit_marker_time_legacy = Cvar_Get("scr_hit_marker_time", scr_hit_marker_time->string, CVAR_NOARCHIVE);
    
    scr_damage_indicators = Cvar_Get("cl_damage_indicators", "1", 0);
    scr_damage_indicators_legacy = Cvar_Get("scr_damage_indicators", scr_damage_indicators->string, CVAR_NOARCHIVE);
    scr_damage_indicator_time = Cvar_Get("cl_damage_indicator_time", "1000", 0);
    scr_damage_indicator_time_legacy = Cvar_Get("scr_damage_indicator_time",
                                                scr_damage_indicator_time->string, CVAR_NOARCHIVE);

    scr_pois = Cvar_Get("cl_pois", "1", 0);
    scr_pois_legacy = Cvar_Get("scr_pois", scr_pois->string, CVAR_NOARCHIVE);
    scr_poi_edge_frac = Cvar_Get("cl_poi_edge_frac", "0.15", 0);
    scr_poi_edge_frac_legacy = Cvar_Get("scr_poi_edge_frac", scr_poi_edge_frac->string, CVAR_NOARCHIVE);
    scr_poi_max_scale = Cvar_Get("cl_poi_max_scale", "1.0", 0);
    scr_poi_max_scale_legacy = Cvar_Get("scr_poi_max_scale", scr_poi_max_scale->string, CVAR_NOARCHIVE);
    scr_safe_zone = Cvar_Get("cl_safe_zone", "0.02", 0);
    scr_safe_zone_legacy = Cvar_Get("scr_safe_zone", scr_safe_zone->string, CVAR_NOARCHIVE);

    cl_screen_alias_register();
    cl_timeout_changed(scr_chathud_time);

    Cmd_Register(scr_cmds);

    scr_scale_changed(scr_scale);
    SCR_SetCrosshairColor();

    scr.initialized = true;
}

void SCR_Shutdown(void)
{
    Cmd_Deregister(scr_cmds);
    if (scr.ui_font && scr.ui_font != scr.font) {
        Font_Free(scr.ui_font);
        scr.ui_font = nullptr;
    }
    if (scr.font) {
        Font_Free(scr.font);
        scr.font = nullptr;
    }
    scr.font_pic = 0;
    scr.ui_font_pic = 0;
    scr.initialized = false;
}

//=============================================================================

/*
================
SCR_BeginLoadingPlaque
================
*/
void SCR_BeginLoadingPlaque(void)
{
    if (!cls.state) {
        return;
    }

    S_StopAllSounds();
    OGG_Update();

    if (cls.disable_screen) {
        return;
    }

#if USE_DEBUG
    if (developer->integer) {
        return;
    }
#endif

    // if at console or menu, don't bring up the plaque
    if (cls.key_dest & (KEY_CONSOLE | KEY_MENU)) {
        return;
    }

    scr.draw_loading = true;
    SCR_UpdateScreen();

    cls.disable_screen = Sys_Milliseconds();
}

/*
================
SCR_EndLoadingPlaque
================
*/
void SCR_EndLoadingPlaque(void)
{
    if (!cls.state) {
        return;
    }
    cls.disable_screen = 0;
    Con_ClearNotify_f();
}

// Clear any parts of the tiled background that were drawn on last frame
static void SCR_TileClear(void)
{
    int top, bottom, left, right;
    float inv_scale;

    //if (con.currentHeight == 1)
    //  return;     // full screen console

    if (scr_viewsize->integer == 100)
        return;     // full screen rendering

    inv_scale = scr.virtual_scale > 0.0f ? (1.0f / scr.virtual_scale) : 1.0f;

    top = Q_rint(scr.vrect.y * inv_scale);
    bottom = Q_rint((scr.vrect.y + scr.vrect.height) * inv_scale);
    left = Q_rint(scr.vrect.x * inv_scale);
    right = Q_rint((scr.vrect.x + scr.vrect.width) * inv_scale);

    // clear above view screen
    R_TileClear(0, 0, scr.hud_width, top, scr.backtile_pic);

    // clear below view screen
    R_TileClear(0, bottom, scr.hud_width,
                scr.hud_height - bottom, scr.backtile_pic);

    // clear left of view screen
    R_TileClear(0, top, left, bottom - top, scr.backtile_pic);

    // clear right of view screen
    R_TileClear(right, top, scr.hud_width - right,
                bottom - top, scr.backtile_pic);
}

//=============================================================================

static void SCR_DrawPause(color_t base_color)
{
    int x, y, w, h;

    if (!sv_paused->integer)
        return;
    if (!cl_paused->integer)
        return;
    if (scr_showpause->integer != 1)
        return;

    R_GetPicSize(&w, &h, scr.pause_pic);
    x = (scr.hud_width - w) / 2;
    y = (scr.hud_height - h) / 2;

    R_DrawPic(x, y, base_color, scr.pause_pic);
}

static void SCR_DrawLoading(void)
{
    int x, y, w, h;

    if (!scr.draw_loading)
        return;

    scr.draw_loading = false;

    R_SetScale(scr.hud_scale);

    R_GetPicSize(&w, &h, scr.loading_pic);
    x = (Q_rint(scr.virtual_width * scr.hud_scale) - w) / 2;
    y = (Q_rint(scr.virtual_height * scr.hud_scale) - h) / 2;

    R_DrawPic(x, y, COLOR_WHITE, scr.loading_pic);

    R_SetScale(1.0f);
}

static void SCR_DrawHitMarker(color_t base_color)
{
    if (!cl.hit_marker_count)
        return;
    if (!scr.hit_marker_pic || scr_hit_marker_time->integer <= 0 ||
        cls.realtime - cl.hit_marker_time > scr_hit_marker_time->integer) {
        cl.hit_marker_count = 0;
        return;
    }

    float frac = (float)(cls.realtime - cl.hit_marker_time) / scr_hit_marker_time->integer;
    float alpha = 1.0f - (frac * frac);

    float scale = max(1.0f, 1.5f * (1.f - frac));
    int w = scr.hit_marker_width * scale;
    int h = scr.hit_marker_height * scale;

    int x = (scr.hud_width - w) / 2;
    int y = (scr.hud_height - h) / 2;

    color_t color = COLOR_RGBA(255, 0, 0, base_color.a * alpha);

    R_DrawStretchPic(x + ch_x->integer,
                     y + ch_y->integer,
                     w,
                     h,
                     color,
                 scr.hit_marker_pic);
}

static scr_damage_entry_t *SCR_AllocDamageDisplay(const vec3_t dir)
{
    scr_damage_entry_t *entry = scr.damage_entries;

    for (int i = 0; i < MAX_DAMAGE_ENTRIES; i++, entry++) {
        if (entry->time <= cls.realtime) {
            goto new_entry;
        }

        float dot = DotProduct(entry->dir, dir);

        if (dot >= 0.95f) {
            return entry;
        }
    }

    entry = scr.damage_entries;

new_entry:
    entry->damage = 0;
    VectorClear(entry->color);
    return entry;
}

void SCR_AddToDamageDisplay(int damage, const vec3_t color, const vec3_t dir)
{
    if (cgame && cgame->DrawCrosshair && cgame->AddDamageDisplay) {
        cgame->AddDamageDisplay(0, damage, color, dir);
        return;
    }

    if (!scr_damage_indicators->integer) {
        return;
    }

    scr_damage_entry_t *entry = SCR_AllocDamageDisplay(dir);

    entry->damage += damage;
    VectorAdd(entry->color, color, entry->color);
    VectorNormalize(entry->color);
    VectorCopy(dir, entry->dir);
    entry->time = cls.realtime + scr_damage_indicator_time->integer;
}

static void SCR_DrawDamageDisplays(color_t base_color)
{
    for (int i = 0; i < MAX_DAMAGE_ENTRIES; i++) {
        scr_damage_entry_t *entry = &scr.damage_entries[i];

        if (entry->time <= cls.realtime)
            continue;

        float frac = (entry->time - cls.realtime) / scr_damage_indicator_time->value;

        float my_yaw = cl.predicted_angles[YAW];
        vec3_t angles;
        vectoangles2(entry->dir, angles);
        float damage_yaw = angles[YAW];
        float yaw_diff = DEG2RAD((my_yaw - damage_yaw) - 180);

        color_t color = COLOR_RGBA(
            (int) (entry->color[0] * 255.f),
            (int) (entry->color[1] * 255.f),
            (int) (entry->color[2] * 255.f),
            (int) (frac * base_color.a)
        );

        int x = scr.hud_width / 2;
        int y = scr.hud_height / 2;

        int size = min(scr.damage_display_width, (DAMAGE_ENTRY_BASE_SIZE * entry->damage));

        R_DrawStretchRotatePic(x, y, size, scr.damage_display_height, color, yaw_diff,
            0, -(scr.crosshair_height + (scr.damage_display_height / 2)), scr.damage_display_pic);
    }
}

void SCR_RemovePOI(int id)
{
    if (cgame && cgame->DrawCrosshair && cgame->RemovePOI) {
        cgame->RemovePOI(0, id);
        return;
    }

    if (!scr_pois->integer)
        return;

    if (id == 0) {
        Com_WPrintf("tried to remove unkeyed POI\n");
        return;
    }
    
    scr_poi_t *poi = &scr.pois[0];

    for (int i = 0; i < MAX_TRACKED_POIS; i++, poi++) {

        if (poi->id == id) {
            poi->id = 0;
            poi->time = 0;
            break;
        }
    }
}

void SCR_AddPOI(int id, int time, const vec3_t p, int image, int color, int flags)
{
    if (cgame && cgame->DrawCrosshair && cgame->AddPOI) {
        cgame->AddPOI(0, id, time, p, image, color, flags);
        return;
    }

    if (!scr_pois->integer)
        return;

    scr_poi_t *poi = NULL;

    if (id == 0) {
        // find any free non-key'd POI. we'll find
        // the oldest POI as a fallback to replace.
    
        scr_poi_t *oldest_poi = NULL, *poi_rover = &scr.pois[0];

        for (int i = 0; i < MAX_TRACKED_POIS; i++, poi_rover++) {
            // not expired
            if (poi_rover->time > cl.time) {
                // keyed
                if (poi_rover->id) {
                    continue;
                } else if (!oldest_poi || poi_rover->time < oldest_poi->time) {
                    oldest_poi = poi_rover;
                }
            } else {
                // expired
                poi = poi_rover;
                break;
            }
        }

        if (!poi) {
            poi = oldest_poi;
        }

    } else {
        // we must replace a matching POI with the ID
        // if one exists, otherwise we pick a free POI,
        // and finally we pick the oldest non-key'd POI.

        scr_poi_t *oldest_poi = NULL;
        scr_poi_t *free_poi = NULL;
        scr_poi_t *poi_rover = &scr.pois[0];

        for (int i = 0; i < MAX_TRACKED_POIS; i++, poi_rover++) {
            // found matching ID, just re-use that one
            if (poi_rover->id == id) {
                poi = poi_rover;
                break;
            }

            if (poi_rover->time <= cl.time) {
                // expired
                if (!free_poi) {
                    free_poi = poi_rover;
                }
            } else {
                // not expired; we should only ever replace non-key'd POIs
                if (!poi_rover->id) {
                    if (!oldest_poi || poi_rover->time < oldest_poi->time) {
                        oldest_poi = poi_rover;
                    }
                }
            }
        }

        if (!poi) {
            poi = free_poi ? free_poi : oldest_poi;
        }
    }

    if (!poi) {
        Com_WPrintf("couldn't add a POI\n");
    }

    poi->id = id;
    poi->time = cl.time + time;
    VectorCopy(p, poi->position);
    poi->image = cl.image_precache[image];
    R_GetPicSize(&poi->width, &poi->height, poi->image);
    poi->color = color;
    poi->flags = flags;
}

#if !USE_EXTERNAL_RENDERERS
extern uint32_t d_8to24table[256];
#endif

typedef enum
{
    POI_FLAG_NONE = 0,
    POI_FLAG_HIDE_ON_AIM = 1, // hide the POI if we get close to it with our aim
} svc_poi_flags;

static void SCR_DrawPOIs(color_t base_color)
{
    if (!scr_pois->integer)
        return;
#if USE_EXTERNAL_RENDERERS
    const uint32_t *palette = re.PaletteTable;
    if (!palette) {
        return;
    }
#endif

    float projection_matrix[16];
    Matrix_Frustum(cl.refdef.fov_x, cl.refdef.fov_y, 1.0f, 0.01f, 8192.f, projection_matrix);

    float view_matrix[16];
    vec3_t viewaxis[3];
    AnglesToAxis(cl.predicted_angles, viewaxis);
    Matrix_FromOriginAxis(cl.refdef.vieworg, viewaxis, view_matrix);

    Matrix_Multiply(projection_matrix, view_matrix, projection_matrix);
    
    scr_poi_t *poi = &scr.pois[0];

    float max_height = scr.hud_height * 0.75f;

    for (int i = 0; i < MAX_TRACKED_POIS; i++, poi++) {

        if (poi->time <= cl.time) {
            continue;
        }

        // https://www.khronos.org/opengl/wiki/GluProject_and_gluUnProject_code
        vec4_t sp = { poi->position[0], poi->position[1], poi->position[2], 1.0f };
        Matrix_TransformVec4(sp, projection_matrix, sp);

        bool behind = sp[3] < 0.f;

        if (sp[3]) {
            sp[3] = 1.0f / sp[3];
            VectorScale(sp, sp[3], sp);
        }

        sp[0] = ((sp[0] * 0.5f) + 0.5f) * scr.hud_width;
        sp[1] = ((-sp[1] * 0.5f) + 0.5f) * scr.hud_height;

        if (behind) {
            sp[0] = scr.hud_width - sp[0];
            sp[1] = scr.hud_height - sp[1];

            if (sp[1] > 0) {
                if (sp[0] < scr.hud_width / 2)
                    sp[0] = 0;
                else
                    sp[0] = scr.hud_width - 1;

                sp[1] = min(sp[1], max_height);
            }
        }

        // scale the icon if they are closer to the edges of the screen
        float scale = 1.0f;

        if (scr_poi_max_scale->value != 1.0f) {
            float edge_dist = min(scr.hud_width, scr.hud_height) * scr_poi_edge_frac->value;

            for (int x = 0; x < 2; x++) {
                float extent = ((x == 0) ? scr.hud_width : scr.hud_height);
                float frac;

                if (sp[x] < edge_dist) {
                    frac = (sp[x] / edge_dist);
                } else if (sp[x] > extent - edge_dist) {
                    frac = (extent - sp[x]) / edge_dist;
                } else {
                    continue;
                }

                scale = Q_clipf(1.0f + (1.0f - frac) * (scr_poi_max_scale->value - 1.f), scale, scr_poi_max_scale->value);
            }
        }

        // center & clamp
        int hw = (poi->width * scale) / 2;
        int hh = (poi->height * scale) / 2;
        
        sp[0] -= hw;
        sp[1] -= hh;
        
        sp[0] = Q_clipf(sp[0], 0, scr.hud_width - hw);
        sp[1] = Q_clipf(sp[1], 0, scr.hud_height - hh);

#if USE_EXTERNAL_RENDERERS
        color_t c = { .u32 = palette[poi->color] };
#else
        color_t c = { .u32 = d_8to24table[poi->color] };
#endif

        // calculate alpha if necessary
        if (poi->flags & POI_FLAG_HIDE_ON_AIM) {
            vec3_t centered = { (scr.hud_width / 2) - sp[0], (scr.hud_height / 2) - sp[1], 0.f };
            sp[2] = 0.f;
            float len = VectorLength(centered);
            c.a = base_color.a * Q_clipf(len / (hw * 6), 0.25f, 1.0f);
        } else {
            c.a = base_color.a;
        }

        R_DrawStretchPic(sp[0], sp[1], hw, hh, c, poi->image);
    }
}

static void SCR_DrawCrosshair(color_t base_color)
{
    int x, y;
    int raw_w, raw_h;
    int base_w, base_h;
    int w, h;
    int ui_scale;
    float pulse_scale = 1.0f;

    if (!scr_crosshair->integer)
        return;
    if (cl.frame.ps.stats[STAT_LAYOUTS] & (LAYOUTS_HIDE_HUD | LAYOUTS_HIDE_CROSSHAIR))
        return;

    SCR_DrawPOIs(base_color);
    SCR_UpdateCrosshairPickupPulse();

    ui_scale = SCR_GetUiScaleInt();

    R_GetPicSize(&raw_w, &raw_h, scr.crosshair_pic);
    if (raw_w < 1 || raw_h < 1)
        return;

    float crosshair_size = cl_crosshair_size ? Cvar_ClampValue(cl_crosshair_size, 1.0f, 512.0f) : 32.0f;
    int max_dim = max(raw_w, raw_h);
    float scale = (float)max_dim > 0 ? (crosshair_size / (float)max_dim) : 1.0f;

    base_w = Q_rint((float)raw_w * ui_scale * scale);
    base_h = Q_rint((float)raw_h * ui_scale * scale);

    if (base_w < 1)
        base_w = 1;
    if (base_h < 1)
        base_h = 1;

    scr.crosshair_width = base_w / ui_scale;
    scr.crosshair_height = base_h / ui_scale;

    if (scr.crosshair_width < 1)
        scr.crosshair_width = 1;
    if (scr.crosshair_height < 1)
        scr.crosshair_height = 1;

    int hit_style = cl_crosshair_hit_style ? Cvar_ClampInteger(cl_crosshair_hit_style, 0, 8) : 0;
    int hit_time = cl_crosshair_hit_time ? Cvar_ClampInteger(cl_crosshair_hit_time, 0, 10000) : 0;
    bool hit_active = false;

    if (hit_style > 0 && hit_time > 0 && cl.crosshair_hit_time && cls.realtime >= cl.crosshair_hit_time) {
        unsigned delta = cls.realtime - cl.crosshair_hit_time;
        if (delta <= (unsigned)hit_time)
            hit_active = true;
    }

    if (cl_crosshair_pulse && cl_crosshair_pulse->integer) {
        pulse_scale = max(pulse_scale, SCR_CalcPickupPulseScale(
            scr_crosshair_pulse_time, CROSSHAIR_PULSE_TIME_MS));
    }

    if (hit_active && (hit_style == 3 || hit_style == 4 || hit_style == 5 ||
                       hit_style == 6 || hit_style == 7 || hit_style == 8)) {
        float amplitude = (hit_style >= 6) ? CROSSHAIR_PULSE_SMALL : CROSSHAIR_PULSE_LARGE;
        pulse_scale = max(pulse_scale, SCR_CalcCrosshairPulseScale(
            cl.crosshair_hit_time, (unsigned)hit_time, amplitude));
    }

    w = Q_rint(base_w * pulse_scale);
    h = Q_rint(base_h * pulse_scale);

    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;

    x = (r_config.width - w) / 2 + (ch_x->integer * ui_scale);
    y = (r_config.height - h) / 2 + (ch_y->integer * ui_scale);

    color_t crosshair_color = scr.crosshair_color;
    if (hit_active) {
        color_t hit_color;
        bool override_color = false;

        if (hit_style == 1 || hit_style == 4 || hit_style == 7) {
            hit_color = SCR_GetCrosshairDamageColor(cl.crosshair_hit_damage);
            override_color = true;
        } else if (hit_style == 2 || hit_style == 5 || hit_style == 8) {
            int index = cl_crosshair_hit_color ? Cvar_ClampInteger(
                cl_crosshair_hit_color, 1, (int)q_countof(ql_crosshair_colors)) : 1;
            hit_color = SCR_GetCrosshairPaletteColor(index);
            override_color = true;
        }

        if (override_color) {
            crosshair_color = SCR_ApplyCrosshairBrightness(hit_color);
        }
    }

    crosshair_color.a = (crosshair_color.a * base_color.a) / 255;

    R_SetScale((float)SCR_GetBaseScaleInt());
    R_DrawStretchPic(x, y, w, h, crosshair_color, scr.crosshair_pic);
    R_SetScale(scr.hud_scale);

    SCR_DrawHitMarker(crosshair_color);

    SCR_DrawDamageDisplays(crosshair_color);
}

static void SCR_Draw2D(void)
{
    if (scr_draw2d->integer <= 0)
        return;     // turn off for screenshots

    if (cls.key_dest & KEY_MENU)
        return;

    R_SetScale(scr.hud_scale);

    scr.hud_height = Q_rint(scr.virtual_height * scr.hud_scale);
    scr.hud_width = Q_rint(scr.virtual_width * scr.hud_scale);
    
    // the rest of 2D elements share common alpha
    color_t color = COLOR_SETA_F(COLOR_WHITE, Cvar_ClampValue(scr_alpha, 0, 1));

    // crosshair has its own color and alpha
    if (cgame && cgame->DrawCrosshair) {
        cgame->DrawCrosshair(0, &cl.frame.ps);
    } else {
        SCR_DrawCrosshair(color);
    }

    if (scr_timegraph->integer)
        SCR_DebugGraph(cls.frametime * 300, 0xdc);

    if (scr_debuggraph->integer || scr_timegraph->integer || scr_netgraph->integer)
        SCR_DrawDebugGraph();

    /* Draw cgame HUD elements */
    vrect_t hud_rect = {0, 0, scr.hud_width, scr.hud_height};
    int safe_x = static_cast<int>(hud_rect.width * scr_safe_zone->value);
    int safe_y = static_cast<int>(hud_rect.height * scr_safe_zone->value);
    vrect_t hud_safe = {safe_x, safe_y, 0, 0};
    cgame->DrawHUD(0, &cl.cgame_data, hud_rect, hud_safe, 1, 0, &cl.frame.ps);

    SCR_DrawNet(color);

    SCR_DrawObjects(color);

    if (cgame && cgame->DrawChatHUD) {
        cgame->DrawChatHUD(0, hud_rect, hud_safe, 1);
    } else {
        SCR_DrawChatHUD(color);
    }

    SCR_DrawTurtle(color);

    SCR_DrawPause(color);

    // debug stats have no alpha

#if USE_DEBUG
    SCR_DrawDebugStats();
    SCR_DrawDebugPmove();
#endif

    R_SetScale(1.0f);
}

static void SCR_DrawActive(void)
{
    // if full screen menu is up, do nothing at all
    if (!UI_IsTransparent())
        return;

    // setup virtual HUD canvas for any 2D draws
    scr.canvas_width = r_config.width;
    scr.canvas_height = r_config.height;
    if (scr.virtual_width == 0 || scr.virtual_height == 0)
        SCR_UpdateVirtualScreen();
    scr.hud_height = scr.virtual_height;
    scr.hud_width = scr.virtual_width;

    // draw black background if not active
    if (cls.state < ca_active) {
        R_DrawFill8(0, 0, scr.hud_width, scr.hud_height, 0);
        return;
    }

    if (cls.state == ca_cinematic) {
        SCR_DrawCinematic();
        return;
    }

    SCR_DrawDemo();

    SCR_CalcVrect();

    // clear any dirty part of the background
    SCR_TileClear();

    // draw 3D game view
    V_RenderView();

    SCR_LagometerAddFrameInfo();

    // draw all 2D elements
    SCR_Draw2D();
}

//=======================================================

/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.
==================
*/
void SCR_UpdateScreen(void)
{
    static int recursive;

    if (!scr.initialized) {
        return;             // not initialized yet
    }

    // if the screen is disabled (loading plaque is up), do nothing at all
    if (cls.disable_screen) {
        unsigned delta = Sys_Milliseconds() - cls.disable_screen;

        if (delta < 120 * 1000) {
            return;
        }

        cls.disable_screen = 0;
        Com_Printf("Loading plaque timed out.\n");
    }

    if (recursive > 1) {
        Com_Error(ERR_FATAL, "%s: recursively called", __func__);
    }

    recursive++;

    R_BeginFrame();

    // do 3D renderer drawing
    SCR_DrawActive();

    // draw main menu
    UI_Draw(cls.realtime);

    // draw console
    Con_DrawConsole();

    // draw loading plaque
    SCR_DrawLoading();

    R_EndFrame();

    recursive--;
}
