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

#include "client.h"
#include "common/loc.h"

#define WEAPON_BAR_DISABLED      0
#define WEAPON_BAR_STATIC_LEFT   1
#define WEAPON_BAR_STATIC_RIGHT  2
#define WEAPON_BAR_STATIC_CENTER 3
#define WEAPON_BAR_TIMED_Q3      4
#define WEAPON_BAR_TIMED_Q2R     5

#define WEAPON_BAR_ICON_SIZE (24 + 2)
#define WEAPON_BAR_PAD       2
#define WEAPON_BAR_SIDE_MARGIN 16
#define WEAPON_BAR_SIDE_CENTER_FRAC 0.5f
#define WEAPON_BAR_CENTER_FRAC_Y 0.79f
#define WEAPON_BAR_STATIC_SCALE 0.5f
#define WEAPON_BAR_NAME_OFFSET 22
#define WEAPON_BAR_AMMO_ABOVE_FRAC 0.625f
#define WEAPON_BAR_TILE_INSET 2

#define WEAPON_BAR_TILE_BG COLOR_RGBA(96, 96, 96, 160)

static cvar_t *wb_screen_frac_y;
static cvar_t *wb_timeout;
static cvar_t *wb_lock_time;
static cvar_t *wb_ammo_scale;
static int weapon_bar_last_mode = WEAPON_BAR_TIMED_Q2R;

/* Draw item/ammo count for weapon bar. */
static void draw_count(int x, int y, float scale, int flags, color_t color, int value)
{
    // Compute an integer text scale factor
    int scale_factor = ((1.f / scr.hud_scale) * scale) + 0.5f;
    if (scale_factor < 1)
        scale_factor = 1;

    // Scale manually, as SCR_DrawStringStretch() can't scale below 100%
    R_SetScale(1.f / scale_factor);

    float coord_scale = 1.f / (scale_factor * scr.hud_scale);
    SCR_DrawString(x * coord_scale, y * coord_scale, flags, color, va("%i", value));

    R_SetScale(scr.hud_scale);
}

static void WeaponBar_GetScaledSize(qhandle_t pic, float scale, int *out_w, int *out_h);

static float WeaponBar_GetSafeFrac(void)
{
    float safe = Cvar_VariableValue("cl_safe_zone");

    return Q_clipf(safe, 0.0f, 0.5f);
}

static void WeaponBar_GetSafeRect(int *out_x, int *out_y, int *out_w, int *out_h)
{
    float safe_frac = WeaponBar_GetSafeFrac();
    int inset_x = Q_rint(scr.hud_width * safe_frac);
    int inset_y = Q_rint(scr.hud_height * safe_frac);

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

static int WeaponBar_ClampStartInRange(int start, int size, int min_pos, int max_pos)
{
    int max_start = max(min_pos, max_pos - size);

    if (start < min_pos)
        return min_pos;

    if (start > max_start)
        return max_start;

    return start;
}

static void WeaponBar_GetScaledCharSize(float scale, int *out_w, int *out_h)
{
    if (scale <= 0.0f)
        scale = 1.0f;

    if (out_w)
        *out_w = max(1, Q_rint(CONCHAR_WIDTH * scale));
    if (out_h)
        *out_h = max(1, Q_rint(CONCHAR_HEIGHT * scale));
}

static int WeaponBar_GetScaledTextWidth(const char *text, float scale)
{
    if (!text || !*text)
        return 0;

    int char_w = 0;
    WeaponBar_GetScaledCharSize(scale, &char_w, NULL);
    return (int)strlen(text) * char_w;
}

static void WeaponBar_DrawScaledString(int x, int y, float scale, int flags, color_t color, const char *text)
{
    if (!text || !*text)
        return;

    int char_w = 0;
    int char_h = 0;
    WeaponBar_GetScaledCharSize(scale, &char_w, &char_h);

    int len = (int)strlen(text);

    if ((flags & UI_CENTER) == UI_CENTER) {
        x -= (len * char_w) / 2;
    } else if (flags & UI_RIGHT) {
        x -= len * char_w;
    }

    for (int i = 0; i < len; i++, x += char_w) {
        R_DrawStretchChar(x, y, char_w, char_h, flags, text[i], color, scr.ui_font_pic);
    }
}

static int WeaponBar_FormatCount(int value, char *out, size_t out_size)
{
    int len = Q_scnprintf(out, out_size, "%i", value);
    if (len < 0)
        return 0;

    return len;
}

static void WeaponBar_GetMaxIconSize(float scale, int *out_w, int *out_h)
{
    int max_w = 0;
    int max_h = 0;

    for (int i = 0; i < cl.weapon_bar.num_slots; i++) {
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.weapon_bar.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        max_w = max(max_w, icon_w);
        max_h = max(max_h, icon_h);
    }

    if (out_w)
        *out_w = max_w;
    if (out_h)
        *out_h = max_h;
}

static void WeaponBar_GetScaledSize(qhandle_t pic, float scale, int *out_w, int *out_h)
{
    int w = 0;
    int h = 0;

    R_GetPicSize(&w, &h, pic);
    if (w <= 0)
        w = WEAPON_BAR_ICON_SIZE;
    if (h <= 0)
        h = WEAPON_BAR_ICON_SIZE;

    if (out_w)
        *out_w = max(1, Q_rint(w * scale));
    if (out_h)
        *out_h = max(1, Q_rint(h * scale));
}

static void WeaponBar_DrawPicShadowScaled(int x, int y, qhandle_t pic, float scale, int shadow_offset, int *out_w, int *out_h)
{
    int draw_w = 0;
    int draw_h = 0;
    int offset = max(1, Q_rint(shadow_offset * scale));

    WeaponBar_GetScaledSize(pic, scale, &draw_w, &draw_h);
    R_DrawStretchPic(x + offset, y + offset, draw_w, draw_h, COLOR_BLACK, pic);
    R_DrawStretchPic(x, y, draw_w, draw_h, COLOR_WHITE, pic);

    if (out_w)
        *out_w = draw_w;
    if (out_h)
        *out_h = draw_h;
}

static void WeaponBar_DrawSelectionScaled(int icon_x, int icon_y, int icon_w, int icon_h, float scale)
{
    int sel_w = 0;
    int sel_h = 0;

    if (!scr.weapon_bar_selected)
        return;

    WeaponBar_GetScaledSize(scr.weapon_bar_selected, scale, &sel_w, &sel_h);
    if (sel_w <= 0 || sel_h <= 0)
        return;

    int sel_x = icon_x + ((icon_w - sel_w) / 2);
    int sel_y = icon_y + ((icon_h - sel_h) / 2);
    R_DrawStretchPic(sel_x, sel_y, sel_w, sel_h, COLOR_WHITE, scr.weapon_bar_selected);
}

static void R_DrawPicShadow(int x, int y, qhandle_t pic, int shadow_offset)
{
    R_DrawPic(x + shadow_offset, y + shadow_offset, COLOR_BLACK, pic);
    R_DrawPic(x, y, COLOR_WHITE, pic);
}

static int WeaponBar_ClampMode(int mode)
{
    if (mode < WEAPON_BAR_DISABLED || mode > WEAPON_BAR_TIMED_Q2R)
        return WEAPON_BAR_TIMED_Q2R;

    return mode;
}

static int WeaponBar_GetMode(void)
{
    if (!cl_weapon_bar)
        return WEAPON_BAR_TIMED_Q2R;

    return WeaponBar_ClampMode(cl_weapon_bar->integer);
}

static void WeaponBar_ApplyMode(int mode)
{
    if (mode == weapon_bar_last_mode)
        return;

    cl.weapon_bar.state = WHEEL_CLOSED;
    cl.weapon_bar.selected = -1;
    cl.weapon_bar.close_time = 0;
    weapon_bar_last_mode = mode;
}

static void WeaponBar_Close(void)
{
    cl.weapon_bar.state = WHEEL_CLOSED;
}

static void WeaponBar_SetSelectedFromActive(void)
{
    int active = cl.frame.ps.stats[STAT_ACTIVE_WEAPON];
    if (active >= 0 && active < cl.wheel_data.num_weapons)
        cl.weapon_bar.selected = cl.wheel_data.weapons[active].item_index;
    else
        cl.weapon_bar.selected = -1;
}

// populate slot list with stuff we own.
// runs every frame and when we open the weapon bar.
static bool WeaponBar_Populate(bool prefer_active, bool strict_selection)
{
    int i;

    cl.weapon_bar.num_slots = 0;

    int owned = cgame->GetOwnedWeaponWheelWeapons(&cl.frame.ps);

    for (i = 0; i < cl.wheel_data.num_weapons; i++) {
        if (!(owned & BIT(i)))
            continue;

        cl.weapon_bar.slots[cl.weapon_bar.num_slots].data_id = i;
        cl.weapon_bar.slots[cl.weapon_bar.num_slots].has_ammo = cl.wheel_data.weapons[i].ammo_index == -1 ||
            cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, cl.wheel_data.weapons[i].ammo_index);
        cl.weapon_bar.slots[cl.weapon_bar.num_slots].item_index = cl.wheel_data.weapons[i].item_index;
        cl.weapon_bar.num_slots++;
    }

    if (!cl.weapon_bar.num_slots)
        return false;

    if (prefer_active)
        WeaponBar_SetSelectedFromActive();

    // check that we still have the item being selected
    if (cl.weapon_bar.selected == -1) {
        cl.weapon_bar.selected = cl.weapon_bar.slots[0].item_index;
    } else {
        for (i = 0; i < cl.weapon_bar.num_slots; i++)
            if (cl.weapon_bar.slots[i].item_index == cl.weapon_bar.selected)
                break;
    }

    if (i == cl.weapon_bar.num_slots) {
        if (strict_selection)
            return false;

        cl.weapon_bar.selected = cl.weapon_bar.slots[0].item_index;
    }

    return true;
}

static void WeaponBar_Open(void)
{
    if (cl.weapon_bar.state == WHEEL_CLOSED) {
        WeaponBar_SetSelectedFromActive();
    }

    cl.weapon_bar.state = WHEEL_OPEN;

    if (!WeaponBar_Populate(false, true)) {
        WeaponBar_Close();
    }
}

static bool WeaponBar_AdvanceSelection(int offset)
{
    if (cl.weapon_bar.num_slots < 2)
        return false;

    for (int i = 0; i < cl.weapon_bar.num_slots; i++) {
        if (cl.weapon_bar.slots[i].item_index != cl.weapon_bar.selected)
            continue;

        for (int n = 0, o = i + offset; n < cl.weapon_bar.num_slots - 1; n++, o += offset) {
            if (o < 0)
                o = cl.weapon_bar.num_slots - 1;
            else if (o >= cl.weapon_bar.num_slots)
                o = 0;

            if (!cl.weapon_bar.slots[o].has_ammo)
                continue;

            cl.weapon_bar.selected = cl.weapon_bar.slots[o].item_index;
            return true;
        }

        break;
    }

    return false;
}

static color_t WeaponBar_CountColor(const cl_wheel_weapon_t *weap, bool selected, int count)
{
    if (count <= weap->quantity_warn)
        return selected ? COLOR_RGB(255, 83, 83) : COLOR_RED;

    return selected ? COLOR_RGB(255, 255, 83) : COLOR_WHITE;
}

static void WeaponBar_DrawHorizontalScaled(int bar_y, int selected_item, bool draw_ammo, bool ammo_above, float scale)
{
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;
    WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int pad = max(1, Q_rint(WEAPON_BAR_PAD * scale));
    int shadow_offset = max(1, Q_rint(2 * scale));
    int bar_w = 0;
    int center_x = safe_x + (safe_w / 2);
    float ammo_scale = (wb_ammo_scale ? wb_ammo_scale->value : 0.66f) * scale;
    float ammo_effective_scale = max(1.0f, ammo_scale);
    int count_h = max(1, Q_rint(CONCHAR_HEIGHT * ammo_effective_scale));
    int max_icon_h = 0;

    for (int i = 0; i < cl.weapon_bar.num_slots; i++) {
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.weapon_bar.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        bar_w += icon_w;
        max_icon_h = max(max_icon_h, icon_h);
        if (i < (int)cl.weapon_bar.num_slots - 1)
            bar_w += pad;
    }

    int bar_x = center_x - (bar_w / 2);
    bar_x = WeaponBar_ClampStartInRange(bar_x, bar_w, safe_x, safe_x + safe_w);
    bar_y = WeaponBar_ClampStartInRange(bar_y, max_icon_h, safe_y, safe_y + safe_h);

    for (int i = 0; i < cl.weapon_bar.num_slots; i++) {
        bool selected = cl.weapon_bar.slots[i].item_index == selected_item;
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.weapon_bar.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        WeaponBar_DrawPicShadowScaled(bar_x, bar_y, selected ? icons->selected : icons->wheel, scale, shadow_offset, &icon_w, &icon_h);

        if (selected)
            WeaponBar_DrawSelectionScaled(bar_x, bar_y, icon_w, icon_h, scale);

        if (draw_ammo && weap->ammo_index >= 0) {
            int count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weap->ammo_index);
            color_t color = WeaponBar_CountColor(weap, selected, count);
            int ammo_pad = max(1, Q_rint(2 * scale));
            int ammo_offset = max(max(1, Q_rint(icon_h * WEAPON_BAR_AMMO_ABOVE_FRAC)), count_h + ammo_pad);
            int count_y = ammo_above ? (bar_y - ammo_offset) : (bar_y + icon_h + ammo_pad);

            draw_count(bar_x + (icon_w / 2),
                       count_y,
                       ammo_scale,
                       UI_DROPSHADOW | UI_CENTER,
                       color,
                       count);
        }

        bar_x += icon_w + pad;
    }
}

typedef struct weapon_bar_tile_metrics_s {
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
} weapon_bar_tile_metrics_t;

static void WeaponBar_GetStaticTileMetrics(float scale, weapon_bar_tile_metrics_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));

    out->pad = max(1, Q_rint(WEAPON_BAR_PAD * scale));
    out->inset = max(1, Q_rint(WEAPON_BAR_TILE_INSET * scale));
    out->shadow_offset = max(1, Q_rint(2 * scale));

    WeaponBar_GetScaledCharSize(scale, &out->char_w, &out->char_h);

    for (int i = 0; i < cl.weapon_bar.num_slots; i++) {
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.weapon_bar.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;
        int icon_w = 0;
        int icon_h = 0;

        WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        out->max_icon_w = max(out->max_icon_w, icon_w);
        out->max_icon_h = max(out->max_icon_h, icon_h);

        if (weap->ammo_index >= 0) {
            int count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weap->ammo_index);
            char count_text[16];
            WeaponBar_FormatCount(count, count_text, sizeof(count_text));
            out->max_text_w = max(out->max_text_w, WeaponBar_GetScaledTextWidth(count_text, scale));
        }
    }

    out->tile_w = out->max_icon_w + out->max_text_w + (out->pad + (out->inset * 2));
    out->tile_h = max(out->max_icon_h, out->char_h) + (out->inset * 2);
}

static void WeaponBar_DrawStaticTilesVertical(int selected_item, bool right_side)
{
    float scale = WEAPON_BAR_STATIC_SCALE;
    weapon_bar_tile_metrics_t metrics;
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;

    WeaponBar_GetStaticTileMetrics(scale, &metrics);
    WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int total_h = (int)cl.weapon_bar.num_slots * metrics.tile_h + ((int)cl.weapon_bar.num_slots - 1) * metrics.pad;
    int start_y = safe_y + Q_rint((float)safe_h * WEAPON_BAR_SIDE_CENTER_FRAC) - (total_h / 2);
    start_y = WeaponBar_ClampStartInRange(start_y, total_h, safe_y, safe_y + safe_h);

    int start_x = right_side ? (safe_x + safe_w - metrics.tile_w) : safe_x;
    start_x = WeaponBar_ClampStartInRange(start_x, metrics.tile_w, safe_x, safe_x + safe_w);
    int text_flags = UI_DROPSHADOW | (right_side ? UI_RIGHT : 0);

    for (int i = 0; i < cl.weapon_bar.num_slots; i++) {
        bool selected = cl.weapon_bar.slots[i].item_index == selected_item;
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.weapon_bar.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;
        int tile_x = start_x;
        int tile_y = start_y + (i * (metrics.tile_h + metrics.pad));

        if (selected)
            R_DrawFill32(tile_x, tile_y, metrics.tile_w, metrics.tile_h, WEAPON_BAR_TILE_BG);

        int icon_w = 0;
        int icon_h = 0;
        WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        int icon_x = right_side ? (tile_x + metrics.tile_w - metrics.inset - icon_w) : (tile_x + metrics.inset);
        int icon_y = tile_y + metrics.inset + ((metrics.tile_h - (metrics.inset * 2) - icon_h) / 2);

        WeaponBar_DrawPicShadowScaled(icon_x, icon_y, selected ? icons->selected : icons->wheel, scale, metrics.shadow_offset, &icon_w, &icon_h);

        if (selected)
            WeaponBar_DrawSelectionScaled(icon_x, icon_y, icon_w, icon_h, scale);

        if (weap->ammo_index >= 0) {
            int count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weap->ammo_index);
            color_t color = WeaponBar_CountColor(weap, selected, count);
            char count_text[16];
            WeaponBar_FormatCount(count, count_text, sizeof(count_text));

            int text_x = right_side ? (icon_x - metrics.pad) : (tile_x + metrics.inset + metrics.max_icon_w + metrics.pad);
            int text_y = tile_y + ((metrics.tile_h - metrics.char_h) / 2);

            WeaponBar_DrawScaledString(text_x, text_y, scale, text_flags, color, count_text);
        }
    }
}

static void WeaponBar_DrawStaticTilesHorizontal(int selected_item)
{
    float scale = WEAPON_BAR_STATIC_SCALE;
    weapon_bar_tile_metrics_t metrics;
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;

    WeaponBar_GetStaticTileMetrics(scale, &metrics);
    WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int total_w = (int)cl.weapon_bar.num_slots * metrics.tile_w + ((int)cl.weapon_bar.num_slots - 1) * metrics.pad;
    int start_x = safe_x + (safe_w / 2) - (total_w / 2);
    int start_y = safe_y + Q_rint(safe_h * WEAPON_BAR_CENTER_FRAC_Y) + (metrics.tile_h / 2);
    start_x = WeaponBar_ClampStartInRange(start_x, total_w, safe_x, safe_x + safe_w);
    start_y = WeaponBar_ClampStartInRange(start_y, metrics.tile_h, safe_y, safe_y + safe_h);

    int text_flags = UI_DROPSHADOW;

    for (int i = 0; i < cl.weapon_bar.num_slots; i++) {
        bool selected = cl.weapon_bar.slots[i].item_index == selected_item;
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.weapon_bar.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;
        int tile_x = start_x + (i * (metrics.tile_w + metrics.pad));
        int tile_y = start_y;

        if (selected)
            R_DrawFill32(tile_x, tile_y, metrics.tile_w, metrics.tile_h, WEAPON_BAR_TILE_BG);

        int icon_w = 0;
        int icon_h = 0;
        int icon_x = tile_x + metrics.inset;
        WeaponBar_GetScaledSize(icons->wheel, scale, &icon_w, &icon_h);
        int icon_y = tile_y + metrics.inset + ((metrics.tile_h - (metrics.inset * 2) - icon_h) / 2);

        WeaponBar_DrawPicShadowScaled(icon_x, icon_y, selected ? icons->selected : icons->wheel, scale, metrics.shadow_offset, &icon_w, &icon_h);

        if (selected)
            WeaponBar_DrawSelectionScaled(icon_x, icon_y, icon_w, icon_h, scale);

        if (weap->ammo_index >= 0) {
            int count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weap->ammo_index);
            color_t color = WeaponBar_CountColor(weap, selected, count);
            char count_text[16];
            WeaponBar_FormatCount(count, count_text, sizeof(count_text));

            int text_x = tile_x + metrics.inset + metrics.max_icon_w + metrics.pad;
            int text_y = tile_y + ((metrics.tile_h - metrics.char_h) / 2);

            WeaponBar_DrawScaledString(text_x, text_y, scale, text_flags, color, count_text);
        }
    }
}

static void WeaponBar_DrawQ2RTimed(void)
{
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;
    WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int bar_w = cl.weapon_bar.num_slots * (WEAPON_BAR_ICON_SIZE + WEAPON_BAR_PAD);
    int center_x = safe_x + (safe_w / 2);
    int bar_x = center_x - (bar_w / 2);
    int bar_y = safe_y + Q_rint(safe_h * wb_screen_frac_y->value);
    float ammo_scale = wb_ammo_scale ? wb_ammo_scale->value : 0.66f;
    int max_icon_h = 0;

    WeaponBar_GetMaxIconSize(1.0f, NULL, &max_icon_h);
    bar_x = WeaponBar_ClampStartInRange(bar_x, bar_w, safe_x, safe_x + safe_w);
    bar_y = WeaponBar_ClampStartInRange(bar_y, max_icon_h, safe_y, safe_y + safe_h);

    for (int i = 0; i < cl.weapon_bar.num_slots; i++, bar_x += WEAPON_BAR_ICON_SIZE + WEAPON_BAR_PAD) {
        bool selected = cl.weapon_bar.selected == cl.weapon_bar.slots[i].item_index;
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.weapon_bar.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;

        R_DrawPicShadow(bar_x, bar_y, selected ? icons->selected : icons->wheel, 2);

        if (selected) {
            R_DrawPic(bar_x - 1, bar_y - 1, COLOR_WHITE, scr.weapon_bar_selected);

            char localized[CS_MAX_STRING_LENGTH];

            // TODO: cache localized item names in cl somewhere.
            // make sure they get reset of language is changed.
            Loc_Localize(cl.configstrings[cl.csr.items + cl.weapon_bar.slots[i].item_index], false, NULL, 0, localized, sizeof(localized));

            int name_y = max(safe_y, bar_y - 16);
            SCR_DrawString(center_x, name_y, UI_CENTER | UI_DROPSHADOW, COLOR_WHITE, localized);
        }

        if (weap->ammo_index >= 0) {
            int count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weap->ammo_index);
            color_t color = WeaponBar_CountColor(weap, selected, count);

            draw_count(bar_x + (WEAPON_BAR_ICON_SIZE / 2),
                       bar_y + WEAPON_BAR_ICON_SIZE + 2,
                       ammo_scale,
                       UI_DROPSHADOW | UI_CENTER,
                       color,
                       count);
        }
    }
}

static void WeaponBar_DrawQ3Timed(int selected_item)
{
    int safe_x = 0;
    int safe_y = 0;
    int safe_w = 0;
    int safe_h = 0;
    WeaponBar_GetSafeRect(&safe_x, &safe_y, &safe_w, &safe_h);

    int bar_y = safe_y + Q_rint(safe_h * WEAPON_BAR_CENTER_FRAC_Y);
    int center_x = safe_x + (safe_w / 2);

    WeaponBar_DrawHorizontalScaled(bar_y, selected_item, false, false, 1.0f);

    if (selected_item >= 0) {
        char localized[CS_MAX_STRING_LENGTH];

        // TODO: cache localized item names in cl somewhere.
        // make sure they get reset of language is changed.
        Loc_Localize(cl.configstrings[cl.csr.items + selected_item], false, NULL, 0, localized, sizeof(localized));
        if (localized[0]) {
            int name_y = max(safe_y, bar_y - WEAPON_BAR_NAME_OFFSET);
            SCR_DrawString(center_x, name_y, UI_CENTER | UI_DROPSHADOW, COLOR_WHITE, localized);
        }
    }
}

void CL_WeaponBar_Draw(void)
{
    int mode = WeaponBar_GetMode();

    WeaponBar_ApplyMode(mode);

    if (mode == WEAPON_BAR_DISABLED)
        return;

    if (mode == WEAPON_BAR_TIMED_Q2R && cl.weapon_bar.state != WHEEL_OPEN)
        return;

    if (mode == WEAPON_BAR_TIMED_Q3 && cl.weapon_bar.state != WHEEL_OPEN)
        return;

    bool prefer_active = (mode == WEAPON_BAR_STATIC_LEFT || mode == WEAPON_BAR_STATIC_RIGHT || mode == WEAPON_BAR_STATIC_CENTER);
    bool strict_selection = (mode == WEAPON_BAR_TIMED_Q2R);

    if (!WeaponBar_Populate(prefer_active, strict_selection))
        return;

    int selected_item = cl.weapon_bar.selected;
    if (prefer_active || selected_item < 0) {
        int active = cl.frame.ps.stats[STAT_ACTIVE_WEAPON];
        if (active >= 0 && active < cl.wheel_data.num_weapons)
            selected_item = cl.wheel_data.weapons[active].item_index;
    }

    switch (mode) {
    case WEAPON_BAR_STATIC_LEFT:
        WeaponBar_DrawStaticTilesVertical(selected_item, false);
        break;
    case WEAPON_BAR_STATIC_RIGHT:
        WeaponBar_DrawStaticTilesVertical(selected_item, true);
        break;
    case WEAPON_BAR_STATIC_CENTER:
        WeaponBar_DrawStaticTilesHorizontal(selected_item);
        break;
    case WEAPON_BAR_TIMED_Q3:
        WeaponBar_DrawQ3Timed(selected_item);
        break;
    case WEAPON_BAR_TIMED_Q2R:
        WeaponBar_DrawQ2RTimed();
        break;
    default:
        break;
    }
}

void CL_WeaponBar_Input(void)
{
    int mode = WeaponBar_GetMode();

    WeaponBar_ApplyMode(mode);

    if (mode == WEAPON_BAR_TIMED_Q2R) {
        if (cl.weapon_bar.state != WHEEL_OPEN) {
            if (cl.weapon_bar.state == WHEEL_CLOSING && com_localTime3 >= cl.weapon_bar.close_time)
                cl.weapon_bar.state = WHEEL_CLOSED;

            return;
        }

        if (!WeaponBar_Populate(false, true)) {
            WeaponBar_Close();
            return;
        }

        // always holster while open
        cl.cmd.buttons |= BUTTON_HOLSTER;

        if (com_localTime3 >= cl.weapon_bar.close_time || (cl.cmd.buttons & BUTTON_ATTACK)) {
            int active = cl.frame.ps.stats[STAT_ACTIVE_WEAPON];
            if (active >= 0 && active < cl.wheel_data.num_weapons &&
                cl.weapon_bar.selected == cl.wheel_data.weapons[active].item_index) {
                WeaponBar_Close();
                return;
            }

            // switch
            CL_ClientCommand(va("use_index_only %i\n", cl.weapon_bar.selected));
            cl.weapon_bar.state = WHEEL_CLOSING;
            cl.weapon_lock_time = cl.time + wb_lock_time->integer;
        }

        return;
    }

    if (mode == WEAPON_BAR_DISABLED)
        return;

    bool prefer_active = (mode == WEAPON_BAR_STATIC_LEFT || mode == WEAPON_BAR_STATIC_RIGHT || mode == WEAPON_BAR_STATIC_CENTER);

    if (!WeaponBar_Populate(prefer_active, false)) {
        WeaponBar_Close();
        return;
    }

    if (mode == WEAPON_BAR_TIMED_Q3 && cl.weapon_bar.state == WHEEL_OPEN &&
        com_localTime3 >= cl.weapon_bar.close_time) {
        WeaponBar_Close();
    }
}

void CL_WeaponBar_ClearInput(void)
{
    if (WeaponBar_GetMode() != WEAPON_BAR_TIMED_Q2R)
        return;

    if (cl.weapon_bar.state == WHEEL_CLOSING) {
        cl.weapon_bar.state = WHEEL_CLOSED;
        cl.weapon_bar.close_time = com_localTime3 + (cl.frametime.time * 2);
    }
}

void CL_WeaponBar_Cycle(int offset)
{
    int mode = WeaponBar_GetMode();
    bool use_q2r_timed = (mode == WEAPON_BAR_TIMED_Q2R);

    WeaponBar_ApplyMode(mode);

    if (use_q2r_timed) {
        if (cl.weapon_bar.state != WHEEL_OPEN) {
            WeaponBar_Open();
        } else if (!WeaponBar_Populate(false, true)) {
            WeaponBar_Close();
            return;
        }

        WeaponBar_AdvanceSelection(offset);
        cl.weapon_bar.close_time = com_localTime3 + wb_timeout->integer;
        return;
    }

    if (!WeaponBar_Populate(true, false)) {
        WeaponBar_Close();
        return;
    }

    int previous = cl.weapon_bar.selected;
    if (!WeaponBar_AdvanceSelection(offset))
        return;

    if (cl.weapon_bar.selected != previous) {
        CL_ClientCommand(va("use_index_only %i\n", cl.weapon_bar.selected));
        cl.weapon_lock_time = cl.time + wb_lock_time->integer;
    }

    if (mode == WEAPON_BAR_TIMED_Q3) {
        cl.weapon_bar.state = WHEEL_OPEN;
        cl.weapon_bar.close_time = com_localTime3 + wb_timeout->integer;
    }
}

void CL_WeaponBar_Precache(void)
{
    scr.weapon_bar_selected = R_RegisterPic("carousel/selected");
}

void CL_WeaponBar_Init(void)
{
    wb_screen_frac_y = Cvar_Get("wc_screen_frac_y", "0.72", 0);
    wb_timeout = Cvar_Get("wc_timeout", "400", 0);
    wb_lock_time = Cvar_Get("wc_lock_time", "300", 0);
    wb_ammo_scale = Cvar_Get("wc_ammo_scale", "0.66", 0);

    cl.weapon_bar.state = WHEEL_CLOSED;
    cl.weapon_bar.selected = -1;
    cl.weapon_bar.close_time = 0;
    cl.weapon_bar.num_slots = 0;
}
