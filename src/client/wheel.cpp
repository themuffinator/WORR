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

/* Draw item/ammo count for carousel and weapons/items wheel.
 * Notably, the count should be drawn a bit smaller than usual
 * text, but never below 100% */
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

static cvar_t *wc_screen_frac_y;
static cvar_t *wc_timeout;
static cvar_t *wc_lock_time;
static cvar_t *wc_ammo_scale;

static void CL_Carousel_Close(void)
{
    cl.carousel.state = WHEEL_CLOSED;
}

// populate slot list with stuff we own.
// runs every frame and when we open the carousel.
static bool CL_Carousel_Populate(void)
{
    int i;

    cl.carousel.num_slots = 0;

    int owned = cgame->GetOwnedWeaponWheelWeapons(&cl.frame.ps);

    for (i = 0; i < cl.wheel_data.num_weapons; i++) {
        if (!(owned & BIT(i)))
            continue;

        cl.carousel.slots[cl.carousel.num_slots].data_id = i;
        cl.carousel.slots[cl.carousel.num_slots].has_ammo = cl.wheel_data.weapons[i].ammo_index == -1 || cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, cl.wheel_data.weapons[i].ammo_index);
        cl.carousel.slots[cl.carousel.num_slots].item_index = cl.wheel_data.weapons[i].item_index;
        cl.carousel.num_slots++;
    }

    // todo: sort by sort_id

    // todo: cl.wheel.powerups

    if (!cl.carousel.num_slots)
        return false;

    // check that we still have the item being selected
    if (cl.carousel.selected == -1) {
        cl.carousel.selected = cl.carousel.slots[0].item_index;
    } else {
        for (i = 0; i < cl.carousel.num_slots; i++)
            if (cl.carousel.slots[i].item_index == cl.carousel.selected)
                break;
    }

    if (i == cl.carousel.num_slots) {
        // TODO: maybe something smarter?
        return false;
    }

    return true;
}

static void CL_Carousel_Open(void)
{
    if (cl.carousel.state == WHEEL_CLOSED) {
        cl.carousel.selected = (cl.frame.ps.stats[STAT_ACTIVE_WEAPON] == -1) ? -1 : cl.wheel_data.weapons[cl.frame.ps.stats[STAT_ACTIVE_WEAPON]].item_index;
    }

    cl.carousel.state = WHEEL_OPEN;

    if (!CL_Carousel_Populate()) {
        CL_Carousel_Close();
    }
}

#define CAROUSEL_ICON_SIZE (24 + 2)
#define CAROUSEL_PAD       2

static void R_DrawStretchPicShadowAlpha(int x, int y, int w, int h, qhandle_t pic, int shadow_offset, float alpha)
{
    R_DrawStretchPic(x + shadow_offset, y + shadow_offset, w, h, ColorSetAlpha(COLOR_BLACK, alpha), pic);
    R_DrawStretchPic(x, y, w, h, ColorSetAlpha(COLOR_WHITE, alpha), pic);
}

static void R_DrawPicShadow(int x, int y, qhandle_t pic, int shadow_offset)
{
    R_DrawPic(x + shadow_offset, y + shadow_offset, COLOR_BLACK, pic);
    R_DrawPic(x, y, COLOR_WHITE, pic);
}

void CL_Carousel_Draw(void)
{
    if (cl.carousel.state != WHEEL_OPEN)
        return;

    int carousel_w = cl.carousel.num_slots * (CAROUSEL_ICON_SIZE + CAROUSEL_PAD);
    int center_x = scr.hud_width / 2;
    int carousel_x = center_x - (carousel_w / 2);
    int carousel_y = (int) (scr.hud_height * wc_screen_frac_y->value);
    
    for (int i = 0; i < cl.carousel.num_slots; i++, carousel_x += CAROUSEL_ICON_SIZE + CAROUSEL_PAD) {
        bool selected = cl.carousel.selected == cl.carousel.slots[i].item_index;
        const cl_wheel_weapon_t *weap = &cl.wheel_data.weapons[cl.carousel.slots[i].data_id];
        const cl_wheel_icon_t *icons = &weap->icons;

        R_DrawPicShadow(carousel_x, carousel_y, selected ? icons->selected : icons->wheel, 2);
        
        if (selected) {
            R_DrawPic(carousel_x - 1, carousel_y - 1, COLOR_WHITE, scr.carousel_selected);
            
            char localized[CS_MAX_STRING_LENGTH];

            // TODO: cache localized item names in cl somewhere.
            // make sure they get reset of language is changed.
            Loc_Localize(cl.configstrings[cl.csr.items + cl.carousel.slots[i].item_index], false, NULL, 0, localized, sizeof(localized));

            SCR_DrawString(center_x, carousel_y - 16, UI_CENTER | UI_DROPSHADOW, COLOR_WHITE, localized);
        }

        if (weap->ammo_index >= 0) {
            int count = cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weap->ammo_index);
            color_t color;
            if (count <= weap->quantity_warn)
                color = selected ? ColorRGB(255, 83, 83) : COLOR_RED;
            else
                color = selected ? ColorRGB(255, 255, 83) : COLOR_WHITE;

            draw_count(carousel_x + (CAROUSEL_ICON_SIZE / 2), carousel_y + CAROUSEL_ICON_SIZE + 2, wc_ammo_scale->value, UI_DROPSHADOW | UI_CENTER, color, count);
        }
    }
}

void CL_Carousel_ClearInput(void)
{
    if (cl.carousel.state == WHEEL_CLOSING) {
        cl.carousel.state = WHEEL_CLOSED;
        cl.carousel.close_time = com_localTime3 + (cl.frametime.time * 2);
    }
}

void CL_Carousel_Input(void)
{
    if (cl.carousel.state != WHEEL_OPEN) {
        if (cl.carousel.state == WHEEL_CLOSING && com_localTime3 >= cl.carousel.close_time)
            cl.carousel.state = WHEEL_CLOSED;

        return;
    }

    if (!CL_Carousel_Populate()) {
        CL_Carousel_Close();
        return;
    }

    // always holster while open
    cl.cmd.buttons |= BUTTON_HOLSTER;

    if (com_localTime3 >= cl.carousel.close_time || (cl.cmd.buttons & BUTTON_ATTACK)) {

        // already using this weapon
        if (cl.carousel.selected == cl.wheel_data.weapons[cl.frame.ps.stats[STAT_ACTIVE_WEAPON]].item_index) {
            CL_Carousel_Close();
            return;
        }

        // switch
        CL_ClientCommand(va("use_index_only %i\n", cl.carousel.selected));
        cl.carousel.state = WHEEL_CLOSING;

        cl.weapon_lock_time = cl.time + wc_lock_time->integer;
    }
}

static void CL_Wheel_Cycle(int offset)
{
    if (cl.wheel.state != WHEEL_OPEN) {
        CL_Carousel_Open();
    } else if (!CL_Carousel_Populate()) {
        CL_Carousel_Close();
        return;
    }

    // TODO this is ugly :(
    for (int i = 0; i < cl.carousel.num_slots; i++)
        if (cl.carousel.slots[i].item_index == cl.carousel.selected) {

            for (int n = 0, o = i + offset; n < cl.carousel.num_slots - 1; n++, o += offset) {
                if (o < 0)
                    o = cl.carousel.num_slots - 1;
                else if (o >= cl.carousel.num_slots)
                    o = 0;

                if (!cl.carousel.slots[o].has_ammo)
                    continue;

                cl.carousel.selected = cl.carousel.slots[o].item_index;
                break;
            }

            break;
        }

    cl.carousel.close_time = com_localTime3 + wc_timeout->integer;
}

void CL_Wheel_WeapNext(void) { CL_Wheel_Cycle(1); }
void CL_Wheel_WeapPrev(void) { CL_Wheel_Cycle(-1); }

static cvar_t *ww_timer_speed;
static cvar_t *ww_ammo_scale;

static int wheel_slot_compare(const void *a, const void *b)
{
    const cl_wheel_slot_t *sa = a;
    const cl_wheel_slot_t *sb = b;

    if (sa->sort_id == sb->sort_id)
        return sa->item_index - sb->item_index;

    return sa->sort_id - sb->sort_id;
}

// populate slot list with stuff we own.
// runs every frame and when we open the carousel.
static bool CL_Wheel_Populate(void)
{
    int i;

    cl.wheel.num_slots = 0;

    int owned = cgame->GetOwnedWeaponWheelWeapons(&cl.frame.ps);
    cl_wheel_slot_t *slot = cl.wheel.slots;

    if (cl.wheel.is_powerup_wheel) {
        const cl_wheel_powerup_t *powerup = cl.wheel_data.powerups;

        for (i = 0; i < cl.wheel_data.num_powerups; i++, slot++, cl.wheel.num_slots++, powerup++) {
            slot->data_id = i;
            slot->is_powerup = true;
            slot->has_ammo = powerup->ammo_index == -1 || cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, powerup->ammo_index);
            slot->item_index = powerup->item_index;
            slot->has_item = cgame->GetPowerupWheelCount(&cl.frame.ps, i);
            slot->sort_id = powerup->sort_id;
            slot->icons = &powerup->icons;
        }
    } else {
        const cl_wheel_weapon_t *weapon = cl.wheel_data.weapons;

        for (i = 0; i < cl.wheel_data.num_weapons; i++, slot++, cl.wheel.num_slots++, weapon++) {
            slot->data_id = i;
            slot->has_ammo = weapon->ammo_index == -1 || cgame->GetWeaponWheelAmmoCount(&cl.frame.ps, weapon->ammo_index);
            slot->item_index = weapon->item_index;
            slot->has_item = (owned & BIT(i));
            slot->is_powerup = false;
            slot->sort_id = weapon->sort_id;
            slot->icons = &weapon->icons;
        }
    }

    cl.wheel.slice_deg = ((M_PI * 2) / cl.wheel.num_slots);
    cl.wheel.slice_sin = cosf(cl.wheel.slice_deg / 2);

    qsort(cl.wheel.slots, cl.wheel.num_slots, sizeof(*cl.wheel.slots), wheel_slot_compare);

    return !!cl.wheel.num_slots;
}

void CL_Wheel_Open(bool powerup)
{
    cl.wheel.is_powerup_wheel = powerup;
    cl.wheel.selected = -1;

    if (!CL_Wheel_Populate())
        return;

    cl.wheel.state = WHEEL_OPEN;
    cl.wheel.deselect_time = 0;
    Vector2Clear(cl.wheel.position);
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

void CL_Wheel_Close(bool released)
{
    if (cl.wheel.state != WHEEL_OPEN)
        return;

    cl.wheel.state = WHEEL_CLOSING;

    if (released && cl.wheel.selected != -1)
        CL_ClientCommand(va("use_index_only %i\n", cl.wheel.slots[cl.wheel.selected].item_index));
}

static int get_wheel_draw_size(void)
{
    // Find an integer fraction (or multiple) of the original wheel size to choose draw size
    /* The rerelease weapon wheel image (which we're using) has a 74 pixels padding of fully
     * transparent pixels, ignore those then calculating scaling */
    const int wheel_padding = 74;
    const int wheel_content_size = scr.wheel_size - wheel_padding;
    int real_hud_height = scr.hud_height / scr.hud_scale;
    int wheel_draw_size;
    if (wheel_content_size >= real_hud_height) {
        int wheel_size_div = (2 * wheel_content_size - 1) / real_hud_height;
        wheel_draw_size = scr.wheel_size / wheel_size_div;
    } else {
        int wheel_size_mul = real_hud_height / wheel_content_size;
        wheel_draw_size = scr.wheel_size * wheel_size_mul;
    }
    return wheel_draw_size * scr.hud_scale;
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
    
    if (!CL_Wheel_Populate()) {
        CL_Wheel_Close(false);
        return;
    }

    cl.wheel.position[0] += x;
    cl.wheel.position[1] += y;
    
    // clamp position & calculate dir
    cl.wheel.distance = Vector2Length(cl.wheel.position);
    float inner_size = get_wheel_draw_size() / scr.hud_scale * 0.64f;

    Vector2Clear(cl.wheel.dir);

    if (cl.wheel.distance) {
        float inv_distance = 1.f / cl.wheel.distance;
        Vector2Scale(cl.wheel.position, inv_distance, cl.wheel.dir);

        if (cl.wheel.distance > inner_size / 2) {
            cl.wheel.distance = inner_size / 2;
            Vector2Scale(cl.wheel.dir, inner_size / 2, cl.wheel.position);
        }
    }
}

void CL_Wheel_Update(void)
{
    static unsigned int lastWheelTime;
    unsigned int t = Sys_Milliseconds();
    float frac = (t - lastWheelTime) * 0.001f;
    lastWheelTime = t;

    if (cl.wheel.state != WHEEL_OPEN)
    {
        if (cl.wheel.timer > 0.0f) {
            cl.wheel.timer = max(0.0f, cl.wheel.timer - (frac * ww_timer_speed->value));
        }

        cl.wheel.timescale = max(0.1f, 1.0f - cl.wheel.timer);
        return;
    }
    
    if (cl.wheel.timer < 1.0f) {
        cl.wheel.timer = min(1.0f, cl.wheel.timer + (frac * ww_timer_speed->value));
    }

    cl.wheel.timescale = max(0.1f, 1.0f - cl.wheel.timer);

    // update cached slice parameters
    for (int i = 0; i < cl.wheel.num_slots; i++) {
        if (!cl.wheel.slots[i].has_item)
            continue;

        cl.wheel.slots[i].angle = cl.wheel.slice_deg * i;
        Vector2Set(cl.wheel.slots[i].dir, sinf(cl.wheel.slots[i].angle), -cosf(cl.wheel.slots[i].angle));

        cl.wheel.slots[i].dot = Dot2Product(cl.wheel.dir, cl.wheel.slots[i].dir);
    }

    // check selection stuff
    bool can_select = (cl.wheel.distance > 140);

    if (can_select) {
        for (int i = 0; i < cl.wheel.num_slots; i++) {
            if (!cl.wheel.slots[i].has_item)
                continue;

            if (cl.wheel.slots[i].dot > cl.wheel.slice_sin) {
                cl.wheel.selected = i;
                cl.wheel.deselect_time = 0;
            }
        }
    } else if (cl.wheel.selected) {
        if (!cl.wheel.deselect_time)
            cl.wheel.deselect_time = com_localTime3 + 200;
    }

    if (cl.wheel.deselect_time && cl.wheel.deselect_time < com_localTime3) {
        cl.wheel.selected = -1;
        cl.wheel.deselect_time = 0;
    }
}

static color_t slot_count_color(bool selected, bool warn_low)
{
    color_t count_color;
    if (selected) {
        // "Selected" color is based off the tint for "selected" wheel icons in rerelease
        count_color = warn_low ? ColorRGB(255, 62, 33) : ColorRGB(255, 248, 134);
    } else
        count_color = warn_low ? COLOR_RED : COLOR_WHITE;
    return count_color;
}

static void draw_wheel_slot(int slot_idx, int center_x, int center_y, int wheel_draw_size, float wheel_alpha, int *out_slot_count, bool *out_warn_low)
{
    const cl_wheel_slot_t *slot = &cl.wheel.slots[slot_idx];

    if (!slot->has_item)
        return;

    bool selected = cl.wheel.selected == slot_idx;

    vec2_t p;
    Vector2Scale(slot->dir, (wheel_draw_size / 2) * 0.525f, p);

    bool active = selected;

    float scale = 1.f;

    if (selected)
        scale = 2.f;

    int size = 12 * scale;
    float alpha = 1.0f;

    // powerup activated
    if (slot->is_powerup) {
        if (cl.wheel_data.powerups[slot->data_id].is_toggle) {
            if (cgame->GetPowerupWheelCount(&cl.frame.ps, slot->data_id) == 2)
                active = true;

            if (cl.wheel_data.powerups[slot->data_id].ammo_index != -1 &&
                !slot->has_ammo)
                alpha = 0.5f;
        }
    }

    alpha *= wheel_alpha;

    R_DrawStretchPicShadowAlpha(center_x + p[0] - size, center_y + p[1] - size, size * 2, size * 2, active ? slot->icons->selected : slot->icons->wheel, 4, alpha);

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
        draw_count(center_x + p[0] + size, center_y + p[1] + size, ww_ammo_scale->value * scale, UI_CENTER | UI_DROPSHADOW, ColorSetAlpha(count_color, wheel_alpha), count);
    }

    // We may need it later
    if (out_slot_count)
        *out_slot_count = count;
    if (out_warn_low)
        *out_warn_low = warn_low;
}

void CL_Wheel_Draw(void)
{
    if (cl.wheel.state != WHEEL_OPEN && cl.wheel.timer == 0.0f)
        return;
    
    int center_x = (scr.hud_width / 2);

    if (cl.wheel.is_powerup_wheel)
        center_x -= (scr.hud_width / 4);
    else
        center_x += (scr.hud_width / 4);

    int center_y = scr.hud_height / 2;

    float t = 1.0f - cl.wheel.timer;
    float tween = 0.5f - (cos((t * t) * M_PIf) * 0.5f);
    float wheel_alpha = 1.0f - tween;
    color_t base_color = ColorSetAlpha(COLOR_WHITE, wheel_alpha);
    int wheel_draw_size = get_wheel_draw_size();

    R_DrawStretchPic(center_x - (wheel_draw_size / 2), center_y - (wheel_draw_size / 2), wheel_draw_size, wheel_draw_size, base_color, scr.wheel_circle);

    // Draw all wheel slots, _except_ the current selected one
    for (int i = 0; i < cl.wheel.num_slots; i++) {
        if (i == cl.wheel.selected) continue;

        draw_wheel_slot(i, center_x, center_y, wheel_draw_size, wheel_alpha, NULL, NULL);
    }

    if (cl.wheel.selected >= 0 && cl.wheel.selected < cl.wheel.num_slots) {
        // Draw the selected wheel slot last; If things overlap, at least the selection appears on top
        int count = -1;
        bool warn_low = false;
        draw_wheel_slot(cl.wheel.selected, center_x, center_y, wheel_draw_size, wheel_alpha, &count, &warn_low);

        const cl_wheel_slot_t *slot = &cl.wheel.slots[cl.wheel.selected];
        char localized[CS_MAX_STRING_LENGTH];

        // TODO: cache localized item names in cl somewhere.
        // make sure they get reset of language is changed.
        Loc_Localize(cl.configstrings[cl.csr.items + slot->item_index], false, NULL, 0, localized, sizeof(localized));

        SCR_DrawString(center_x, center_y - (wheel_draw_size / 8), UI_CENTER | UI_DROPSHADOW, base_color, localized);

        int ammo_index;

        if (slot->is_powerup) {
            ammo_index = cl.wheel_data.powerups[slot->data_id].ammo_index;

            if (!cl.wheel_data.powerups[slot->data_id].is_toggle) {
                SCR_DrawString(center_x, center_y, UI_CENTER | UI_DROPSHADOW, base_color, va("%i", cgame->GetPowerupWheelCount(&cl.frame.ps, slot->data_id)));
            }
        } else {
            ammo_index = cl.wheel_data.weapons[slot->data_id].ammo_index;
        }

        if (ammo_index != -1) {
            const cl_wheel_ammo_t *ammo = &cl.wheel_data.ammo[ammo_index];

            R_DrawStretchPicShadowAlpha(center_x - (24 * 3) / 2, center_y - ((24 * 3) / 2), (24 * 3), (24 * 3), ammo->icons.wheel, 2, wheel_alpha);

            color_t color = slot_count_color(false, warn_low); // don't use selected color inside the wheel
            SCR_DrawString(center_x, center_y + 32, UI_CENTER | UI_DROPSHADOW, ColorSetAlpha(color, wheel_alpha), va("%i", count));
        }
    }

    R_SetScale(1);
    int wheel_button_draw_size = (int)(scr.wheel_button_size / scr.hud_scale);
    R_DrawStretchPic((int)((center_x / scr.hud_scale) + cl.wheel.position[0] - (wheel_button_draw_size / 2)),
                     (int)((center_y / scr.hud_scale) + cl.wheel.position[1] - (wheel_button_draw_size / 2)),
                     wheel_button_draw_size,
                     wheel_button_draw_size,
                     ColorSetAlpha(COLOR_WHITE, wheel_alpha * 0.5f), scr.wheel_button);
}

void CL_Wheel_Precache(void)
{
    scr.carousel_selected = R_RegisterPic("carousel/selected");
    scr.wheel_circle = R_RegisterPic("/gfx/weaponwheel.png");
    R_GetPicSize(&scr.wheel_size, &scr.wheel_size, scr.wheel_circle);
    scr.wheel_button = R_RegisterPic("/gfx/wheelbutton.png");
    R_GetPicSize(&scr.wheel_button_size, &scr.wheel_button_size, scr.wheel_button);

    cl.wheel.timescale = 1.0f;
}

void CL_Wheel_Init(void)
{
    wc_screen_frac_y = Cvar_Get("wc_screen_frac_y", "0.72", 0);
    wc_timeout = Cvar_Get("wc_timeout", "400", 0);
    wc_lock_time = Cvar_Get("wc_lock_time", "300", 0);
    wc_ammo_scale = Cvar_Get("wc_ammo_scale", "0.66", 0);

    ww_timer_speed = Cvar_Get("ww_timer_speed", "3", 0);
    ww_ammo_scale = Cvar_Get("ww_ammo_scale", "0.66", 0);

    cl.wheel.timescale = 1.0f;
}