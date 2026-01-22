/*
Copyright (C) 2026

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
#include "client/font.h"
#include "client/ui_font.h"

static font_t *ui_font_handle;
static font_t *ui_list_font_handle;
static cvar_t *ui_font;
static cvar_t *ui_scale;
static float ui_pixel_scale;
static int ui_last_width;
static int ui_last_height;
static const int k_ui_list_font_size = 6;
static const float k_ui_ttf_letter_spacing = 0.06f;

static float UI_FontCalcPixelScale(void)
{
    float scale_x = (float)r_config.width / VIRTUAL_SCREEN_WIDTH;
    float scale_y = (float)r_config.height / VIRTUAL_SCREEN_HEIGHT;
    float base_scale = max(scale_x, scale_y);
    int base_scale_int = (int)base_scale;

    if (base_scale_int < 1)
        base_scale_int = 1;

    float ui_draw_scale = ui_scale ? R_ClampScale(ui_scale) : 1.0f;
    if (ui_draw_scale <= 0.0f)
        ui_draw_scale = 1.0f;

    return (float)base_scale_int / ui_draw_scale;
}

static font_t *UI_FontLoadHandle(const char *path, int line_height)
{
    font_t *handle = Font_Load(path, line_height, ui_pixel_scale, 0,
                               "fonts/qfont.kfont", "conchars.png");

    if (!handle && ui_font && ui_font->default_string &&
        (!path || strcmp(path, ui_font->default_string))) {
        handle = Font_Load(ui_font->default_string, line_height, ui_pixel_scale, 0,
                           "fonts/qfont.kfont", "conchars.png");
    }

    if (handle)
        Font_SetLetterSpacing(handle, k_ui_ttf_letter_spacing);

    return handle;
}

static font_t *UI_FontGetHandleForSize(int size)
{
    if (size == k_ui_list_font_size)
        return ui_list_font_handle ? ui_list_font_handle : ui_font_handle;
    return ui_font_handle;
}

static void UI_FontReload(void)
{
    font_t *old_font = ui_font_handle;
    font_t *old_list = ui_list_font_handle;
    if (old_list && old_list != old_font)
        Font_Free(old_list);
    if (old_font)
        Font_Free(old_font);
    ui_font_handle = nullptr;
    ui_list_font_handle = nullptr;

    ui_pixel_scale = UI_FontCalcPixelScale();
    ui_last_width = r_config.width;
    ui_last_height = r_config.height;

    const char *font_path = ui_font ? ui_font->string : nullptr;
    ui_font_handle = UI_FontLoadHandle(font_path, CONCHAR_HEIGHT);

    if (!ui_font_handle && ui_font && strcmp(ui_font->string, ui_font->default_string)) {
        Cvar_Reset(ui_font);
        ui_font_handle = UI_FontLoadHandle(ui_font->default_string, CONCHAR_HEIGHT);
    }

    ui_list_font_handle = UI_FontLoadHandle(font_path, k_ui_list_font_size);
    if (!ui_list_font_handle)
        ui_list_font_handle = ui_font_handle;
}

static void ui_font_changed(cvar_t *self)
{
    if (!self)
        return;
    UI_FontReload();
}

static void UI_FontEnsure(void)
{
    if (!ui_font) {
        ui_font = Cvar_Get("ui_font", "fonts/NotoSansKR-Regular.otf", 0);
        ui_font->changed = ui_font_changed;
    }

    if (!ui_scale)
        ui_scale = Cvar_Get("ui_scale", "0", 0);

    float pixel_scale = UI_FontCalcPixelScale();
    if (!ui_font_handle ||
        !ui_list_font_handle ||
        ui_pixel_scale != pixel_scale ||
        ui_last_width != r_config.width ||
        ui_last_height != r_config.height) {
        UI_FontReload();
    }
}

void UI_FontInit(void)
{
    UI_FontEnsure();
}

void UI_FontShutdown(void)
{
    font_t *old_font = ui_font_handle;
    font_t *old_list = ui_list_font_handle;
    if (old_list && old_list != old_font)
        Font_Free(old_list);
    if (old_font)
        Font_Free(old_font);
    ui_font_handle = nullptr;
    ui_list_font_handle = nullptr;
    ui_pixel_scale = 0.0f;
    ui_last_width = 0;
    ui_last_height = 0;
}

int UI_FontDrawString(int x, int y, int flags, size_t max_chars,
                      const char *string, color_t color)
{
    if (!string || !*string)
        return x;

    UI_FontEnsure();
    if (!ui_font_handle)
        return x;

    return Font_DrawString(ui_font_handle, x, y, 1, flags, max_chars, string, color);
}

int UI_FontDrawStringSized(int x, int y, int flags, size_t max_chars,
                            const char *string, color_t color, int size)
{
    if (!string || !*string)
        return x;

    UI_FontEnsure();
    font_t *font = UI_FontGetHandleForSize(size);
    if (!font)
        return x;

    return Font_DrawString(font, x, y, 1, flags, max_chars, string, color);
}

int UI_FontMeasureString(int flags, size_t max_chars, const char *string,
                         int *out_height)
{
    if (!string || !*string) {
        if (out_height)
            *out_height = 0;
        return 0;
    }

    UI_FontEnsure();
    if (!ui_font_handle) {
        if (out_height)
            *out_height = 0;
        return 0;
    }

    return Font_MeasureString(ui_font_handle, 1, flags, max_chars, string, out_height);
}

int UI_FontMeasureStringSized(int flags, size_t max_chars, const char *string,
                               int *out_height, int size)
{
    if (!string || !*string) {
        if (out_height)
            *out_height = 0;
        return 0;
    }

    UI_FontEnsure();
    font_t *font = UI_FontGetHandleForSize(size);
    if (!font) {
        if (out_height)
            *out_height = 0;
        return 0;
    }

    return Font_MeasureString(font, 1, flags, max_chars, string, out_height);
}

int UI_FontLineHeight(int scale)
{
    UI_FontEnsure();
    if (!ui_font_handle)
        return CONCHAR_HEIGHT * max(scale, 1);
    return Font_LineHeight(ui_font_handle, scale);
}

int UI_FontLineHeightSized(int size)
{
    UI_FontEnsure();
    font_t *font = UI_FontGetHandleForSize(size);
    if (!font)
        return CONCHAR_HEIGHT;
    return Font_LineHeight(font, 1);
}

qhandle_t UI_FontLegacyHandle(void)
{
    UI_FontEnsure();
    return Font_LegacyHandle(ui_font_handle);
}
