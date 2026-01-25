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

static void draw_scaled_string(int x, int y, float scale, int flags, color_t color, const char *text)
{
    if (!text || !*text)
        return;

    if (scale <= 0.0f)
        scale = 1.0f;

    int char_w = max(1, Q_rint(CONCHAR_WIDTH * scale));
    int char_h = max(1, Q_rint(CONCHAR_HEIGHT * scale));
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

static void draw_wheel_count(int x, int y, float scale, int flags, color_t color, int value)
{
    char buffer[16];

    Q_snprintf(buffer, sizeof(buffer), "%i", value);
    draw_scaled_string(x, y, scale, flags, color, buffer);
}

static void draw_wheel_sb_number(int x, int y, float height, float alpha, int value, bool alt)
{
    char buffer[16];
    int len = Q_scnprintf(buffer, sizeof(buffer), "%i", value);
    if (len <= 0)
        return;

    qhandle_t *digits = scr.sb_pics[alt ? 1 : 0];
    int digit_w, digit_h;
    R_GetPicSize(&digit_w, &digit_h, digits[0]);
    if (digit_w <= 0 || digit_h <= 0)
        return;

    float scale = (height > 0.0f) ? (height / (float)digit_h) : 1.0f;
    int draw_w = max(1, Q_rint(digit_w * scale));
    int draw_h = max(1, Q_rint(digit_h * scale));
    int draw_x = x - (draw_w * len) / 2;
    int draw_y = y - (draw_h / 2);
    color_t color = COLOR_SETA_F(COLOR_WHITE, alpha);

    for (int i = 0; i < len; i++) {
        char c = buffer[i];
        int frame;

        if (c == '-')
            frame = STAT_MINUS;
        else if (c >= '0' && c <= '9')
            frame = c - '0';
        else
            continue;

        R_DrawStretchPic(draw_x, draw_y, draw_w, draw_h, color, digits[frame]);
        draw_x += draw_w;
    }
}

static void R_DrawStretchPicShadowAlpha(int x, int y, int w, int h, qhandle_t pic, int shadow_offset, float alpha)
{
    R_DrawStretchPic(x + shadow_offset, y + shadow_offset, w, h, COLOR_SETA_F(COLOR_BLACK, alpha), pic);
    R_DrawStretchPic(x, y, w, h, COLOR_SETA_F(COLOR_WHITE, alpha), pic);
}
static const cl_wheel_slot_t *CL_Wheel_GetSelectedSlot(void)
{
    if (cl.wheel.selected < 0 || cl.wheel.selected >= (int)cl.wheel.num_slots)
        return NULL;

    const cl_wheel_slot_t *slot = &cl.wheel.slots[cl.wheel.selected];
    if (!slot->has_item)
        return NULL;

    return slot;
}

static bool CL_Wheel_GetItemDropName(int item_index, char *out_name, size_t out_size)
{
    if (!out_name || out_size == 0)
        return false;

    const char *raw = cl.configstrings[cl.csr.items + item_index];
    if (!raw || !*raw)
        return false;

    Loc_Localize(raw, false, NULL, 0, out_name, out_size);
    if (!out_name[0])
        return false;

    return true;
}

static bool CL_Wheel_IsAmmoWeapon(const cl_wheel_weapon_t *weapon)
{
    if (!weapon || weapon->ammo_index < 0)
        return false;

    if (weapon->ammo_index >= cl.wheel_data.num_ammo)
        return false;

    return cl.wheel_data.ammo[weapon->ammo_index].item_index == weapon->item_index;
}

static bool CL_Wheel_CanDropAmmoForSlot(const cl_wheel_slot_t *slot)
{
    if (!slot || slot->is_powerup || !slot->has_item)
        return false;

    const cl_wheel_weapon_t *weapon = &cl.wheel_data.weapons[slot->data_id];
    if (weapon->ammo_index < 0 || weapon->ammo_index >= cl.wheel_data.num_ammo)
        return false;

    int count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weapon->ammo_index);
    if (count <= 0)
        return false;

    const cl_wheel_ammo_t *ammo = &cl.wheel_data.ammo[weapon->ammo_index];
    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CL_Wheel_GetItemDropName(ammo->item_index, drop_name, sizeof(drop_name)))
        return false;

    return true;
}

static bool CL_Wheel_CanDropWeaponForSlot(const cl_wheel_slot_t *slot)
{
    if (!slot || slot->is_powerup || !slot->has_item)
        return false;

    const cl_wheel_weapon_t *weapon = &cl.wheel_data.weapons[slot->data_id];
    if (!weapon->can_drop)
        return false;

    if (CL_Wheel_IsAmmoWeapon(weapon))
        return false;

    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CL_Wheel_GetItemDropName(slot->item_index, drop_name, sizeof(drop_name)))
        return false;

    return true;
}

bool CL_Wheel_DropAmmo(void)
{
    if (cl.wheel.state != WHEEL_OPEN)
        return false;

    if (cl.wheel.is_powerup_wheel)
        return true;

    const cl_wheel_slot_t *slot = CL_Wheel_GetSelectedSlot();
    if (!slot || slot->is_powerup)
        return true;

    if (!CL_Wheel_CanDropAmmoForSlot(slot))
        return true;

    const cl_wheel_weapon_t *weapon = &cl.wheel_data.weapons[slot->data_id];
    const cl_wheel_ammo_t *ammo = &cl.wheel_data.ammo[weapon->ammo_index];
    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CL_Wheel_GetItemDropName(ammo->item_index, drop_name, sizeof(drop_name)))
        return true;

    CL_ClientCommand(va("drop %s\n", drop_name));
    return true;
}

bool CL_Wheel_DropWeapon(void)
{
    if (cl.wheel.state != WHEEL_OPEN)
        return false;

    if (cl.wheel.is_powerup_wheel)
        return true;

    const cl_wheel_slot_t *slot = CL_Wheel_GetSelectedSlot();
    if (!slot || slot->is_powerup)
        return true;

    if (!CL_Wheel_CanDropWeaponForSlot(slot))
        return true;

    char drop_name[CS_MAX_STRING_LENGTH];
    if (!CL_Wheel_GetItemDropName(slot->item_index, drop_name, sizeof(drop_name)))
        return true;

    CL_ClientCommand(va("drop %s\n", drop_name));
    return true;
}

bool CL_Wheel_Attack(void)
{
    if (cl.wheel.state != WHEEL_OPEN)
        return false;

    return true;
}

void CL_Wheel_WeapNext(void)
{
    if (CL_Wheel_DropWeapon())
        return;

    CL_WeaponBar_Cycle(1);
}

void CL_Wheel_WeapPrev(void)
{
    if (CL_Wheel_DropAmmo())
        return;

    CL_WeaponBar_Cycle(-1);
}

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

#define WHEEL_CURSOR_RADIUS     256.0f
#define WHEEL_DEADZONE_RADIUS   144.0f
#define WHEEL_ITEM_RADIUS       200.0f
#define WHEEL_REFERENCE_SIZE    700.0f
#define WHEEL_REFERENCE_HEIGHT  1440.0f
#define WHEEL_REFERENCE_CVAR    175.0f // 700px @ 1440p -> 175 in 16:9 virtual height
#define WHEEL_ICON_BASE_SCALE   1.5f
#define WHEEL_CENTER_COUNT_HEIGHT_FRAC 0.1f

static float CL_Wheel_GetDrawSize(void)
{
    float size_value = ww_size ? ww_size->value : WHEEL_REFERENCE_CVAR;
    size_value = Q_clipf(size_value, 1.0f, 4096.0f);

    float base_size = scr.hud_height * (WHEEL_REFERENCE_SIZE / WHEEL_REFERENCE_HEIGHT);
    return max(1.0f, base_size * (WHEEL_REFERENCE_CVAR / size_value));
}

static float CL_Wheel_GetScale(float draw_size)
{
    if (scr.wheel_size <= 0)
        return 1.0f;

    return draw_size / (float)scr.wheel_size;
}

static void CL_Wheel_GetCenter(float *out_x, float *out_y)
{
    float frac_x = ww_screen_frac_x ? ww_screen_frac_x->value : 0.76f;
    float frac_y = ww_screen_frac_y ? ww_screen_frac_y->value : 0.5f;
    frac_x = Q_clipf(frac_x, 0.0f, 1.0f);
    frac_y = Q_clipf(frac_y, 0.0f, 1.0f);

    float offset = frac_x - 0.5f;
    float center_x = (scr.hud_width * 0.5f) + ((cl.wheel.is_powerup_wheel ? -offset : offset) * scr.hud_width);
    float center_y = scr.hud_height * frac_y;

    if (out_x)
        *out_x = center_x;
    if (out_y)
        *out_y = center_y;
}

static void CL_Wheel_WarpCursor(float center_x, float center_y)
{
    float pixel_scale = scr.hud_scale > 0.0f ? (scr.virtual_scale / scr.hud_scale) : scr.virtual_scale;
    if (pixel_scale <= 0.0f)
        pixel_scale = 1.0f;

    IN_WarpMouse(Q_rint(center_x * pixel_scale), Q_rint(center_y * pixel_scale));
}

static color_t CL_Wheel_SlotColor(bool available, float alpha)
{
    color_t color = COLOR_WHITE;

    if (!available) {
        int shade = ww_unavailable_shade_value ? ww_unavailable_shade_value->integer : 80;
        shade = Q_clip(shade, 0, 255);
        color = COLOR_RGB(shade, shade, shade);
    }

    return COLOR_SETA_F(color, alpha);
}

static void R_DrawStretchPicShadowColor(int x, int y, int w, int h, qhandle_t pic, int shadow_offset, color_t color)
{
    float alpha = color.a / 255.0f;
    R_DrawStretchPic(x + shadow_offset, y + shadow_offset, w, h, COLOR_SETA_F(COLOR_BLACK, alpha), pic);
    R_DrawStretchPic(x, y, w, h, color, pic);
}

typedef struct {
    vec2_t pos;
    color_t color;
} wheel_poly_vert_t;

static void DrawPolygon(const wheel_poly_vert_t *verts, int num)
{
    if (!verts || num != 3)
        return;

    float min_x = min(verts[0].pos[0], min(verts[1].pos[0], verts[2].pos[0]));
    float max_x = max(verts[0].pos[0], max(verts[1].pos[0], verts[2].pos[0]));
    float min_y = min(verts[0].pos[1], min(verts[1].pos[1], verts[2].pos[1]));
    float max_y = max(verts[0].pos[1], max(verts[1].pos[1], verts[2].pos[1]));

    int x0 = max(0, (int)floorf(min_x));
    int x1 = min(scr.hud_width - 1, (int)ceilf(max_x));
    int y0 = max(0, (int)floorf(min_y));
    int y1 = min(scr.hud_height - 1, (int)ceilf(max_y));

    float denom = (verts[1].pos[1] - verts[2].pos[1]) * (verts[0].pos[0] - verts[2].pos[0]) +
                  (verts[2].pos[0] - verts[1].pos[0]) * (verts[0].pos[1] - verts[2].pos[1]);
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
                float u = ((verts[1].pos[1] - verts[2].pos[1]) * (px - verts[2].pos[0]) +
                           (verts[2].pos[0] - verts[1].pos[0]) * (py - verts[2].pos[1])) / denom;
                float v = ((verts[2].pos[1] - verts[0].pos[1]) * (px - verts[2].pos[0]) +
                           (verts[0].pos[0] - verts[2].pos[0]) * (py - verts[2].pos[1])) / denom;
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

            color_t color = COLOR_RGBA((int)Q_clipf(r, 0.0f, 255.0f),
                                       (int)Q_clipf(g, 0.0f, 255.0f),
                                       (int)Q_clipf(b, 0.0f, 255.0f),
                                       (int)Q_clipf(a, 0.0f, 255.0f));
            R_DrawFill32(x, y, 1, 1, color);
        }
    }
}

static void CL_Wheel_DrawArrowTriangle(float center_x, float center_y, const vec2_t dir, float offset, float size, float alpha)
{
    float height = max(2.0f, size);
    float half_width = max(1.0f, height);

    vec2_t tip = { center_x + dir[0] * offset, center_y + dir[1] * offset };
    vec2_t base_center = { tip[0] - dir[0] * height, tip[1] - dir[1] * height };
    vec2_t perp = { -dir[1], dir[0] };

    color_t tip_color = COLOR_SETA_F(COLOR_WHITE, alpha);
    color_t base_color = COLOR_SETA_F(COLOR_RGB(0x43, 0x43, 0x43), alpha);

    wheel_poly_vert_t verts[3];

    // soft edge pass for smoother appearance
    float soften = 1.12f;
    float soft_height = height * soften;
    float soft_half_width = half_width * soften;
    vec2_t soft_base = { tip[0] - dir[0] * soft_height, tip[1] - dir[1] * soft_height };
    color_t soft_tip = COLOR_SETA_F(COLOR_WHITE, alpha * 0.35f);
    color_t soft_base_color = COLOR_SETA_F(COLOR_RGB(0x43, 0x43, 0x43), alpha * 0.35f);

    verts[0].pos[0] = tip[0];
    verts[0].pos[1] = tip[1];
    verts[1].pos[0] = soft_base[0] + perp[0] * soft_half_width;
    verts[1].pos[1] = soft_base[1] + perp[1] * soft_half_width;
    verts[2].pos[0] = soft_base[0] - perp[0] * soft_half_width;
    verts[2].pos[1] = soft_base[1] - perp[1] * soft_half_width;
    verts[0].color = soft_tip;
    verts[1].color = soft_base_color;
    verts[2].color = soft_base_color;
    DrawPolygon(verts, 3);

    verts[0].pos[0] = tip[0];
    verts[0].pos[1] = tip[1];
    verts[1].pos[0] = base_center[0] + perp[0] * half_width;
    verts[1].pos[1] = base_center[1] + perp[1] * half_width;
    verts[2].pos[0] = base_center[0] - perp[0] * half_width;
    verts[2].pos[1] = base_center[1] - perp[1] * half_width;
    verts[0].color = tip_color;
    verts[1].color = base_color;
    verts[2].color = base_color;
    DrawPolygon(verts, 3);
}

static int wheel_slot_compare(const void *a, const void *b)
{
    const cl_wheel_slot_t *sa = a;
    const cl_wheel_slot_t *sb = b;

    if (sa->sort_id == sb->sort_id)
        return sa->item_index - sb->item_index;

    return sa->sort_id - sb->sort_id;
}

static float CL_Wheel_GetPopoutForItem(const cl_wheel_slot_t *slots, size_t num_slots, int item_index)
{
    for (size_t i = 0; i < num_slots; i++) {
        if (slots[i].item_index == item_index)
            return slots[i].popout;
    }

    return 0.0f;
}

static float CL_Wheel_GetScaleForItem(const cl_wheel_slot_t *slots, size_t num_slots, int item_index)
{
    for (size_t i = 0; i < num_slots; i++) {
        if (slots[i].item_index == item_index)
            return slots[i].icon_scale;
    }

    return 1.0f;
}

static float CL_Wheel_GetHoverScale(void)
{
    return 2.0f;
}

static float CL_Wheel_GetHoverSpeed(float selected_scale)
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

static void CL_Wheel_AdvanceValue(float *val, float target, float speed, float dt)
{
    if (speed <= 0.0f || dt <= 0.0f)
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

static bool CL_Wheel_RebuildSlots(bool preserve_state)
{
    cl_wheel_slot_t old_slots[MAX_WHEEL_ITEMS * 2];
    size_t old_num_slots = 0;
    int selected_item = -1;

    if (preserve_state && cl.wheel.num_slots > 0) {
        old_num_slots = cl.wheel.num_slots;
        memcpy(old_slots, cl.wheel.slots, old_num_slots * sizeof(*old_slots));

        if (cl.wheel.selected >= 0 && cl.wheel.selected < (int)cl.wheel.num_slots)
            selected_item = cl.wheel.slots[cl.wheel.selected].item_index;
    }

    cl.wheel.num_slots = 0;

    if (cl.wheel.is_powerup_wheel) {
        const cl_wheel_powerup_t *powerup = cl.wheel_data.powerups;

        for (int i = 0; i < cl.wheel_data.num_powerups; i++, powerup++) {
            int count = cgame->GetPowerupWheelCount(&cl.frame.ps, i);
            if (count <= 0)
                continue;

            cl_wheel_slot_t *slot = &cl.wheel.slots[cl.wheel.num_slots++];
            slot->data_id = i;
            slot->is_powerup = true;
            slot->item_index = powerup->item_index;
            slot->sort_id = powerup->sort_id;
            slot->icons = &powerup->icons;
            slot->has_item = true;
            slot->has_ammo = powerup->ammo_index == -1 ||
                             cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, powerup->ammo_index);
            slot->popout = preserve_state ? CL_Wheel_GetPopoutForItem(old_slots, old_num_slots, slot->item_index) : 0.0f;
            slot->icon_scale = preserve_state ? CL_Wheel_GetScaleForItem(old_slots, old_num_slots, slot->item_index) : 1.0f;
            if (slot->icon_scale <= 0.0f)
                slot->icon_scale = 1.0f;
        }
    } else {
        int owned = cgame->GetOwnedWeaponWheelWeapons(&cl.frame.ps);
        const cl_wheel_weapon_t *weapon = cl.wheel_data.weapons;

        for (int i = 0; i < cl.wheel_data.num_weapons; i++, weapon++) {
            if (!(owned & BIT(i)))
                continue;

            cl_wheel_slot_t *slot = &cl.wheel.slots[cl.wheel.num_slots++];
            slot->data_id = i;
            slot->is_powerup = false;
            slot->item_index = weapon->item_index;
            slot->sort_id = weapon->sort_id;
            slot->icons = &weapon->icons;
            slot->has_item = true;
            slot->has_ammo = weapon->ammo_index == -1 ||
                             cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weapon->ammo_index);
            slot->popout = preserve_state ? CL_Wheel_GetPopoutForItem(old_slots, old_num_slots, slot->item_index) : 0.0f;
            slot->icon_scale = preserve_state ? CL_Wheel_GetScaleForItem(old_slots, old_num_slots, slot->item_index) : 1.0f;
            if (slot->icon_scale <= 0.0f)
                slot->icon_scale = 1.0f;
        }
    }

    if (!cl.wheel.num_slots) {
        cl.wheel.selected = -1;
        cl.wheel.deselect_time = 0;
        return false;
    }

    qsort(cl.wheel.slots, cl.wheel.num_slots, sizeof(*cl.wheel.slots), wheel_slot_compare);

    cl.wheel.slice_deg = ((M_PI * 2) / cl.wheel.num_slots);
    cl.wheel.slice_sin = cosf(cl.wheel.slice_deg / 2);

    cl.wheel.selected = -1;
    if (preserve_state && selected_item != -1) {
        for (int i = 0; i < (int)cl.wheel.num_slots; i++) {
            if (cl.wheel.slots[i].item_index == selected_item) {
                cl.wheel.selected = i;
                break;
            }
        }

        if (cl.wheel.selected == -1)
            cl.wheel.deselect_time = 0;
    }

    return true;
}


void CL_Wheel_Open(bool powerup)
{
    cl.wheel.is_powerup_wheel = powerup;
    cl.wheel.selected = -1;
    cl.wheel.deselect_time = 0;
    Vector2Clear(cl.wheel.position);
    cl.wheel.distance = 0.0f;
    Vector2Clear(cl.wheel.dir);

    if (!CL_Wheel_RebuildSlots(false))
        return;

    cl.wheel.state = WHEEL_OPEN;

    float center_x, center_y;
    CL_Wheel_GetCenter(&center_x, &center_y);
    CL_Wheel_WarpCursor(center_x, center_y);
}

float CL_Wheel_TimeScale(void)
{
    return cl.wheel.timescale;
}

void CL_Wheel_ClearInput(void)
{
    if (cl.wheel.state == WHEEL_CLOSING)
        cl.wheel.state = WHEEL_CLOSED;
}

static void CL_Wheel_UseSelected(void)
{
    if (cl.wheel.selected < 0 || cl.wheel.selected >= (int)cl.wheel.num_slots)
        return;

    const cl_wheel_slot_t *slot = &cl.wheel.slots[cl.wheel.selected];
    if (!slot->has_item)
        return;

    CL_ClientCommand(va("use_index_only %i\n", slot->item_index));
}

void CL_Wheel_Close(bool released)
{
    if (cl.wheel.state != WHEEL_OPEN)
        return;

    cl.wheel.state = WHEEL_CLOSING;
    if (ww_controller_exit_timeout) {
        int timeout = max(0, ww_controller_exit_timeout->integer);
        cl.wheel.input_block_until = com_localTime3 + timeout;
    }

    if (released)
        CL_Wheel_UseSelected();
}

void CL_Wheel_Input(int x, int y)
{
    if (cl.wheel.state == WHEEL_CLOSED)
        return;

    // always holster while open
    if (!cl.wheel.is_powerup_wheel)
        cl.cmd.buttons |= BUTTON_HOLSTER;

    if (cl.wheel.state != WHEEL_OPEN)
        return;

    if (cl.wheel.input_block_until && com_localTime3 < cl.wheel.input_block_until)
        return;

    float wheel_draw_size = CL_Wheel_GetDrawSize();
    float wheel_scale = CL_Wheel_GetScale(wheel_draw_size);
    float deadzone_radius = WHEEL_DEADZONE_RADIUS * wheel_scale;
    float max_radius = WHEEL_CURSOR_RADIUS * wheel_scale;

    float deadzone_speed = ww_mouse_deadzone_speed ? ww_mouse_deadzone_speed->value : 0.5f;
    float move_speed = ww_mouse_sensitivity ? ww_mouse_sensitivity->value : 1.0f;
    float speed = (cl.wheel.distance <= deadzone_radius) ? deadzone_speed : move_speed;
    speed = Q_clipf(speed, 0.0f, 10.0f);

    cl.wheel.position[0] += x * speed;
    cl.wheel.position[1] += y * speed;

    cl.wheel.distance = Vector2Length(cl.wheel.position);
    if (cl.wheel.distance > max_radius) {
        float scale = max_radius / cl.wheel.distance;
        Vector2Scale(cl.wheel.position, scale, cl.wheel.position);
        cl.wheel.distance = max_radius;
    }

    Vector2Clear(cl.wheel.dir);
    if (cl.wheel.distance > 0.0f) {
        float inv_distance = 1.0f / cl.wheel.distance;
        Vector2Scale(cl.wheel.position, inv_distance, cl.wheel.dir);
    }
}

void CL_Wheel_Update(void)
{
    static unsigned int lastWheelTime;
    unsigned int t = Sys_Milliseconds();
    if (!lastWheelTime)
        lastWheelTime = t;
    float frac = (t - lastWheelTime) * 0.001f;
    lastWheelTime = t;
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 0.1f)
        frac = 0.1f;
    float timer_speed = ww_timer_speed ? ww_timer_speed->value : 0.0f;

    if (cl.wheel.state != WHEEL_OPEN)
    {
        if (cl.wheel.timer > 0.0f) {
            cl.wheel.timer = max(0.0f, cl.wheel.timer - (frac * timer_speed));
        }

        cl.wheel.timescale = max(0.1f, 1.0f - cl.wheel.timer);
        return;
    }

    CL_Wheel_RebuildSlots(true);

    if (cl.wheel.timer < 1.0f) {
        cl.wheel.timer = min(1.0f, cl.wheel.timer + (frac * timer_speed));
    }

    cl.wheel.timescale = max(0.1f, 1.0f - cl.wheel.timer);

    if (!cl.wheel.num_slots)
        return;

    float wheel_scale = CL_Wheel_GetScale(CL_Wheel_GetDrawSize());
    float deadzone_radius = WHEEL_DEADZONE_RADIUS * wheel_scale;

    // update cached slice parameters
    for (int i = 0; i < cl.wheel.num_slots; i++) {
        cl.wheel.slots[i].angle = cl.wheel.slice_deg * i;
        Vector2Set(cl.wheel.slots[i].dir, sinf(cl.wheel.slots[i].angle), -cosf(cl.wheel.slots[i].angle));

        cl.wheel.slots[i].dot = Dot2Product(cl.wheel.dir, cl.wheel.slots[i].dir);
    }

    if (cl.wheel.selected >= 0 && cl.wheel.selected < (int)cl.wheel.num_slots) {
        if (!cl.wheel.slots[cl.wheel.selected].has_item) {
            cl.wheel.selected = -1;
            cl.wheel.deselect_time = 0;
        }
    }

    // check selection stuff
    bool can_select = (cl.wheel.distance >= deadzone_radius);

    if (can_select) {
        int best = -1;
        float best_dot = cl.wheel.slice_sin;

        for (int i = 0; i < cl.wheel.num_slots; i++) {
            if (!cl.wheel.slots[i].has_item)
                continue;

            if (cl.wheel.slots[i].dot > best_dot) {
                best = i;
                best_dot = cl.wheel.slots[i].dot;
            }
        }

        if (best != -1) {
            cl.wheel.selected = best;
            cl.wheel.deselect_time = 0;
        }
    } else if (cl.wheel.selected >= 0) {
        if (!cl.wheel.deselect_time)
            cl.wheel.deselect_time = com_localTime3 + max(0, ww_deadzone_timeout ? ww_deadzone_timeout->integer : 0);
    }

    if (cl.wheel.deselect_time && cl.wheel.deselect_time < com_localTime3) {
        cl.wheel.selected = -1;
        cl.wheel.deselect_time = 0;
    }

    float popout_target = ww_popout_amount ? max(0.0f, ww_popout_amount->value) : 0.0f;
    float popout_speed = ww_popout_speed ? max(0.0f, ww_popout_speed->value) : 0.0f;
    float hover_scale = CL_Wheel_GetHoverScale();
    float selected_scale = Q_clipf(hover_scale / WHEEL_ICON_BASE_SCALE, 0.1f, 4.0f);
    float hover_speed = CL_Wheel_GetHoverSpeed(selected_scale);
    for (int i = 0; i < cl.wheel.num_slots; i++) {
        float target = (cl.wheel.selected == i && cl.wheel.slots[i].has_item) ? popout_target : 0.0f;
        CL_Wheel_AdvanceValue(&cl.wheel.slots[i].popout, target, popout_speed, frac);

        float scale_target = (cl.wheel.selected == i && cl.wheel.slots[i].has_item) ? selected_scale : 1.0f;
        if (hover_speed <= 0.0f)
            cl.wheel.slots[i].icon_scale = scale_target;
        else
            CL_Wheel_AdvanceValue(&cl.wheel.slots[i].icon_scale, scale_target, hover_speed, frac);
    }
}

static color_t slot_count_color(bool selected, bool warn_low)
{
    color_t count_color;
    if (selected) {
        // "Selected" color is based off the tint for "selected" wheel icons in rerelease
        count_color = warn_low ? COLOR_RGB(255, 62, 33) : COLOR_RGB(255, 248, 134);
    } else
        count_color = warn_low ? COLOR_RED : COLOR_WHITE;
    return count_color;
}

static void draw_wheel_slot(int slot_idx, float center_x, float center_y, float wheel_scale, float wheel_alpha, int *out_slot_count, bool *out_warn_low)
{
    const cl_wheel_slot_t *slot = &cl.wheel.slots[slot_idx];
    bool selected = cl.wheel.selected == slot_idx;
    bool available = slot->has_item;

    vec2_t p;
    Vector2Scale(slot->dir, (WHEEL_ITEM_RADIUS + slot->popout) * wheel_scale, p);

    bool active = selected;
    float alpha = wheel_alpha;

    // powerup activated
    if (slot->is_powerup) {
        if (cl.wheel_data.powerups[slot->data_id].is_toggle) {
            if (cgame->GetPowerupWheelCount(&cl.frame.ps, slot->data_id) == 2)
                active = true;

            if (cl.wheel_data.powerups[slot->data_id].ammo_index != -1 &&
                !slot->has_ammo)
                alpha *= 0.5f;
        }
    }

    qhandle_t icon = active ? slot->icons->selected : slot->icons->wheel;
    int icon_w, icon_h;
    R_GetPicSize(&icon_w, &icon_h, icon);
    float icon_scale = slot->icon_scale > 0.0f ? slot->icon_scale : 1.0f;
    float base_scale = WHEEL_ICON_BASE_SCALE;
    int draw_w = max(1, Q_rint(icon_w * wheel_scale * base_scale * icon_scale));
    int draw_h = max(1, Q_rint(icon_h * wheel_scale * base_scale * icon_scale));
    int draw_x = Q_rint(center_x + p[0] - (draw_w / 2));
    int draw_y = Q_rint(center_y + p[1] - (draw_h / 2));

    color_t icon_color = CL_Wheel_SlotColor(available, alpha);

    if (selected) {
        float nudge = ww_underpic_nudge_amount ? ww_underpic_nudge_amount->value : 0.0f;
        if (nudge != 0.0f) {
            vec2_t under_offset;
            Vector2Scale(slot->dir, -nudge * wheel_scale, under_offset);
            int under_x = Q_rint(center_x + p[0] + under_offset[0] - (draw_w / 2));
            int under_y = Q_rint(center_y + p[1] + under_offset[1] - (draw_h / 2));
            R_DrawStretchPicShadowColor(under_x, under_y, draw_w, draw_h, slot->icons->wheel, 2,
                                        CL_Wheel_SlotColor(available, alpha * 0.4f));
        }
    }

    R_DrawStretchPicShadowColor(draw_x, draw_y, draw_w, draw_h, icon, 2, icon_color);

    int count = -1;
    bool warn_low = false;

    if (slot->is_powerup) {
        if (!cl.wheel_data.powerups[slot->data_id].is_toggle)
            count = cgame->GetPowerupWheelCount(&cl.frame.ps, slot->data_id);
        else if (cl.wheel_data.powerups[slot->data_id].ammo_index != -1)
            count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, cl.wheel_data.powerups[slot->data_id].ammo_index);
    } else {
        if (cl.wheel_data.weapons[slot->data_id].ammo_index != -1) {
            count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, cl.wheel_data.weapons[slot->data_id].ammo_index);
            warn_low = count <= cl.wheel_data.weapons[slot->data_id].quantity_warn;
        }
    }

    color_t count_color = slot_count_color(selected, warn_low);

    int min_count = slot->is_powerup ? 2 : 0;
    if (count != -1 && count >= min_count) {
        float count_size = (ww_ammo_size ? ww_ammo_size->value : 24.0f) * wheel_scale;
        if (!slot->is_powerup)
            count_size *= 0.4f;
        float count_scale = count_size / (float)CONCHAR_HEIGHT;
        draw_wheel_count(Q_rint(center_x + p[0] + (draw_w / 2)),
                         Q_rint(center_y + p[1] + (draw_h / 2)),
                         count_scale,
                         UI_CENTER | UI_DROPSHADOW,
                         COLOR_SETA_F(count_color, wheel_alpha),
                         count);
    }

    // We may need it later
    if (out_slot_count)
        *out_slot_count = count;
    if (out_warn_low)
        *out_warn_low = warn_low;
}

static void CL_Wheel_DrawDropHints(float center_x, float center_y, float wheel_draw_size, float wheel_alpha)
{
    if (cl.wheel.is_powerup_wheel)
        return;

    const cl_wheel_slot_t *slot = CL_Wheel_GetSelectedSlot();
    if (!slot || slot->is_powerup)
        return;

    bool can_drop_weapon = CL_Wheel_CanDropWeaponForSlot(slot);
    bool can_drop_ammo = CL_Wheel_CanDropAmmoForSlot(slot);
    if (!can_drop_weapon && !can_drop_ammo)
        return;

    float scale = 0.75f;
    int line_height = max(1, Q_rint(CONCHAR_HEIGHT * scale));
    int icon_size = max(1, line_height * 3);
    int padding = max(4, icon_size / 6);
    int line_gap = icon_size + 2;
    float wheel_scale = CL_Wheel_GetScale(wheel_draw_size);
    float deadzone_radius = WHEEL_DEADZONE_RADIUS * wheel_scale;
    int x = Q_rint(center_x - deadzone_radius);
    int y = Q_rint(center_y + (wheel_draw_size * 0.35f)) + (line_height / 2);
    color_t color = COLOR_SETA_F(COLOR_WHITE, wheel_alpha);

    int line_index = 0;
    if (can_drop_weapon) {
        int line_y = y + (line_gap * line_index++);
        int icon_y = line_y - ((icon_size - line_height) / 2);
        const char *key_name = NULL;
        int icon_w = SCR_DrawBindIcon("cl_weapnext", x, icon_y, icon_size, color, &key_name);
        int text_x = x + (icon_w > 0 ? icon_w + padding : 0);
        char line_weap[64];
        if (!key_name || !*key_name)
            Q_snprintf(line_weap, sizeof(line_weap), "<UNBOUND> Drop Weapon");
        else if (icon_w > 0)
            Q_snprintf(line_weap, sizeof(line_weap), "Drop Weapon");
        else
            Q_snprintf(line_weap, sizeof(line_weap), "[%s] Drop Weapon", key_name);
        draw_scaled_string(text_x, line_y, scale, UI_DROPSHADOW, color, line_weap);
    }

    if (can_drop_ammo) {
        int line_y = y + (line_gap * line_index++);
        int icon_y = line_y - ((icon_size - line_height) / 2);
        const char *key_name = NULL;
        int icon_w = SCR_DrawBindIcon("cl_weapprev", x, icon_y, icon_size, color, &key_name);
        int text_x = x + (icon_w > 0 ? icon_w + padding : 0);
        char line_ammo[64];
        if (!key_name || !*key_name)
            Q_snprintf(line_ammo, sizeof(line_ammo), "<UNBOUND> Drop Ammo");
        else if (icon_w > 0)
            Q_snprintf(line_ammo, sizeof(line_ammo), "Drop Ammo");
        else
            Q_snprintf(line_ammo, sizeof(line_ammo), "[%s] Drop Ammo", key_name);
        draw_scaled_string(text_x, line_y, scale, UI_DROPSHADOW, color, line_ammo);
    }
}

void CL_Wheel_Draw(void)
{
    if (cl.wheel.state != WHEEL_OPEN && cl.wheel.timer == 0.0f)
        return;

    float center_x, center_y;
    CL_Wheel_GetCenter(&center_x, &center_y);

    float t = 1.0f - cl.wheel.timer;
    float tween = 0.5f - (cos((t * t) * M_PIf) * 0.5f);
    float wheel_alpha = 1.0f - tween;
    color_t base_color = COLOR_SETA_F(COLOR_WHITE, wheel_alpha);
    color_t wheel_color = COLOR_SETA_F(COLOR_WHITE, wheel_alpha * 0.8f);
    float wheel_draw_size = CL_Wheel_GetDrawSize();
    float wheel_scale = CL_Wheel_GetScale(wheel_draw_size);
    int wheel_draw_size_i = max(1, Q_rint(wheel_draw_size));

    R_DrawStretchPic(Q_rint(center_x - (wheel_draw_size_i / 2)),
                     Q_rint(center_y - (wheel_draw_size_i / 2)),
                     wheel_draw_size_i,
                     wheel_draw_size_i,
                     wheel_color,
                     scr.wheel_circle);

    // Draw all wheel slots, _except_ the current selected one
    for (int i = 0; i < cl.wheel.num_slots; i++) {
        if (i == cl.wheel.selected) continue;

        draw_wheel_slot(i, center_x, center_y, wheel_scale, wheel_alpha, NULL, NULL);
    }

    if (cl.wheel.selected >= 0 && cl.wheel.selected < (int)cl.wheel.num_slots) {
        // Draw the selected wheel slot last; If things overlap, at least the selection appears on top
        int count = -1;
        bool warn_low = false;
        draw_wheel_slot(cl.wheel.selected, center_x, center_y, wheel_scale, wheel_alpha, &count, &warn_low);

        const cl_wheel_slot_t *slot = &cl.wheel.slots[cl.wheel.selected];
        if (slot->has_item) {
            char localized[CS_MAX_STRING_LENGTH];

            // TODO: cache localized item names in cl somewhere.
            // make sure they get reset of language is changed.
            Loc_Localize(cl.configstrings[cl.csr.items + slot->item_index], false, NULL, 0, localized, sizeof(localized));

            int name_y = Q_rint(center_y - (wheel_draw_size * 0.125f) + (CONCHAR_HEIGHT * 3));
            SCR_DrawString(Q_rint(center_x), name_y, UI_CENTER | UI_DROPSHADOW, base_color, localized);

            float arrow_offset = (ww_arrow_offset ? ww_arrow_offset->value : 102.0f) * wheel_scale;
            float arrow_size = (ww_ammo_size ? ww_ammo_size->value : 24.0f) * wheel_scale;
            CL_Wheel_DrawArrowTriangle(center_x, center_y, slot->dir, arrow_offset, arrow_size, wheel_alpha);

            int ammo_index;

            if (slot->is_powerup) {
                ammo_index = cl.wheel_data.powerups[slot->data_id].ammo_index;

                if (!cl.wheel_data.powerups[slot->data_id].is_toggle) {
                    float count_size = max(1.0f, scr.hud_height * WHEEL_CENTER_COUNT_HEIGHT_FRAC);
                    draw_wheel_sb_number(Q_rint(center_x),
                                         Q_rint(center_y),
                                         count_size,
                                         wheel_alpha,
                                         cgame->GetPowerupWheelCount(&cl.frame.ps, slot->data_id),
                                         false);
                }
            } else {
                ammo_index = cl.wheel_data.weapons[slot->data_id].ammo_index;
            }

            if (ammo_index != -1) {
                const cl_wheel_ammo_t *ammo = &cl.wheel_data.ammo[ammo_index];
                int ammo_w, ammo_h;
                R_GetPicSize(&ammo_w, &ammo_h, ammo->icons.wheel);

                int icon_w = max(1, Q_rint(ammo_w * wheel_scale * 3.0f));
                int icon_h = max(1, Q_rint(ammo_h * wheel_scale * 3.0f));
                int icon_x = Q_rint(center_x - (icon_w / 2));
                int icon_y = Q_rint(center_y - (icon_h / 2));
                R_DrawStretchPicShadowAlpha(icon_x, icon_y, icon_w, icon_h, ammo->icons.wheel, 2, wheel_alpha);

                float count_size = max(1.0f, scr.hud_height * WHEEL_CENTER_COUNT_HEIGHT_FRAC);
                int count_y = Q_rint(center_y + (icon_h / 2) + (count_size * 0.5f));
                draw_wheel_sb_number(Q_rint(center_x),
                                     count_y,
                                     count_size,
                                     wheel_alpha,
                                     count,
                                     false);
            }
        }
    }

    int wheel_button_draw_size = max(1, Q_rint(scr.wheel_button_size * wheel_scale));
    R_DrawStretchPic(Q_rint(center_x + cl.wheel.position[0] - (wheel_button_draw_size / 2)),
                     Q_rint(center_y + cl.wheel.position[1] - (wheel_button_draw_size / 2)),
                     wheel_button_draw_size,
                     wheel_button_draw_size,
                     COLOR_SETA_F(COLOR_WHITE, wheel_alpha * 0.5f), scr.wheel_button);

    CL_Wheel_DrawDropHints(center_x, center_y, wheel_draw_size, wheel_alpha);
}

void CL_Wheel_Precache(void)
{
    scr.wheel_circle = R_RegisterPic("/gfx/weaponwheel.png");
    R_GetPicSize(&scr.wheel_size, &scr.wheel_size, scr.wheel_circle);
    scr.wheel_button = R_RegisterPic("/gfx/wheelbutton.png");
    R_GetPicSize(&scr.wheel_button_size, &scr.wheel_button_size, scr.wheel_button);

    cl.wheel.timescale = 1.0f;
}

void CL_Wheel_Init(void)
{
    ww_ammo_size = Cvar_Get("ww_ammo_size", "24.0", CVAR_SERVERINFO);
    ww_arrow_offset = Cvar_Get("ww_arrow_offset", "102.0", CVAR_SERVERINFO);
    ww_controller_exit_timeout = Cvar_Get("ww_controller_exit_timeout", "150", CVAR_USERINFO);
    ww_deadzone_timeout = Cvar_Get("ww_deadzone_timeout", "350", CVAR_USERINFO);
    ww_hover_scale = Cvar_Get("ww_hover_scale", "2.0", CVAR_SERVERINFO);
    ww_hover_time = Cvar_Get("ww_hover_time", "200", CVAR_SERVERINFO);
    ww_mouse_deadzone_speed = Cvar_Get("ww_mouse_deadzone_speed", "0.5", CVAR_SERVERINFO);
    ww_mouse_sensitivity = Cvar_Get("ww_mouse_sensitivity", "0.75", CVAR_SERVERINFO);
    ww_popout_amount = Cvar_Get("ww_popout_amount", "4.0", CVAR_SERVERINFO);
    ww_popout_speed = Cvar_Get("ww_popout_speed", "7.2", CVAR_SERVERINFO);
    ww_screen_frac_x = Cvar_Get("ww_screen_frac_x", "0.76", CVAR_SERVERINFO);
    ww_screen_frac_y = Cvar_Get("ww_screen_frac_y", "0.5", CVAR_SERVERINFO);
    ww_size = Cvar_Get("ww_size", "95.0", CVAR_SERVERINFO);
    ww_timer_speed = Cvar_Get("ww_timer_speed", "3.0", CVAR_SERVERINFO);
    ww_unavailable_shade_value = Cvar_Get("ww_unavailable_shade_value", "80", CVAR_USERINFO);
    ww_underpic_nudge_amount = Cvar_Get("ww_underpic_nudge_amount", "4.0", CVAR_SERVERINFO);

    cl.wheel.timescale = 1.0f;
}
