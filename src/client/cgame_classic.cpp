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

#include "shared/shared.hpp"
#include "cgame_classic.hpp"
#include "client/client.hpp"
#include "client/keys.hpp"

#include "common/cvar.hpp"
#include "common/game3_convert.hpp"
#include "common/game3_pmove.hpp"
#include "common/utils.hpp"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#if HAVE_MALLOC_H
#include <malloc.h>
#endif

/* Some definitions copied from client.h
 * This file is deliberately not included to make sure only functions from
 * the cgame_import struct are used */
#define CONCHAR_WIDTH  8
#define CONCHAR_HEIGHT 8
constexpr int BIND_ICON_TARGET_HEIGHT = CONCHAR_HEIGHT * 3;

#define UI_LEFT             BIT(0)
#define UI_RIGHT            BIT(1)
#define UI_CENTER           (UI_LEFT | UI_RIGHT)

static cgame_import_t cgi;
static cgame_q2pro_extended_support_ext_t cgix;
static const cs_remap_t *csr;
static int max_stats;

namespace
{

struct bind_icon_t
{
    std::string name;
    int width = 0;
    int height = 0;
    int draw_width = 0;
    int draw_height = 0;
};

enum class bind_segment_type_t
{
    text,
    icon
};

struct bind_segment_t
{
    bind_segment_type_t type = bind_segment_type_t::text;
    std::string text;
    bind_icon_t icon;
};

static std::string CGC_FormatKeyName(const char *key_name)
{
    if (!key_name || !*key_name) {
        return std::string("UNBOUND");
    }

    std::string formatted;
    formatted.reserve(strlen(key_name));
    for (const char *p = key_name; *p; ++p) {
        const char ch = *p;
        if (ch == '_') {
            formatted.push_back(' ');
        } else {
            formatted.push_back(static_cast<char>(Q_toupper(static_cast<unsigned char>(ch))));
        }
    }
    return formatted;
}

static bool CGC_KeyNameToKeynum(const char *key_name, int *keynum)
{
    if (!key_name || !*key_name) {
        return false;
    }

    if (!key_name[1]) {
        *keynum = static_cast<unsigned char>(key_name[0]);
        return true;
    }

    struct mapping_t {
        const char *name;
        int keynum;
    };

    static const mapping_t key_mappings[] = {
        {"BACKSPACE", K_BACKSPACE},
        {"TAB", K_TAB},
        {"ENTER", K_ENTER},
        {"RETURN", K_ENTER},
        {"PAUSE", K_PAUSE},
        {"ESCAPE", K_ESCAPE},
        {"ESC", K_ESCAPE},
        {"SPACE", K_SPACE},
        {"DEL", K_DEL},
        {"DELETE", K_DEL},
        {"UPARROW", K_UPARROW},
        {"DOWNARROW", K_DOWNARROW},
        {"LEFTARROW", K_LEFTARROW},
        {"RIGHTARROW", K_RIGHTARROW},
        {"ALT", K_ALT},
        {"CTRL", K_CTRL},
        {"SHIFT", K_SHIFT},
        {"F1", K_F1},
        {"F2", K_F2},
        {"F3", K_F3},
        {"F4", K_F4},
        {"F5", K_F5},
        {"F6", K_F6},
        {"F7", K_F7},
        {"F8", K_F8},
        {"F9", K_F9},
        {"F10", K_F10},
        {"F11", K_F11},
        {"F12", K_F12},
        {"INS", K_INS},
        {"INSERT", K_INS},
        {"PGDN", K_PGDN},
        {"PGUP", K_PGUP},
        {"HOME", K_HOME},
        {"END", K_END},
        {"102ND", K_102ND},
        {"NUMLOCK", K_NUMLOCK},
        {"CAPSLOCK", K_CAPSLOCK},
        {"SCROLLOCK", K_SCROLLOCK},
        {"LWINKEY", K_LWINKEY},
        {"RWINKEY", K_RWINKEY},
        {"MENU", K_MENU},
        {"PRINTSCREEN", K_PRINTSCREEN},
        {"KP_HOME", K_KP_HOME},
        {"KP_UPARROW", K_KP_UPARROW},
        {"KP_PGUP", K_KP_PGUP},
        {"KP_LEFTARROW", K_KP_LEFTARROW},
        {"KP_5", K_KP_5},
        {"KP_RIGHTARROW", K_KP_RIGHTARROW},
        {"KP_END", K_KP_END},
        {"KP_DOWNARROW", K_KP_DOWNARROW},
        {"KP_PGDN", K_KP_PGDN},
        {"KP_ENTER", K_KP_ENTER},
        {"KP_INS", K_KP_INS},
        {"KP_DEL", K_KP_DEL},
        {"KP_SLASH", K_KP_SLASH},
        {"KP_MINUS", K_KP_MINUS},
        {"KP_PLUS", K_KP_PLUS},
        {"KP_MULTIPLY", K_KP_MULTIPLY},
        {"LALT", K_LALT},
        {"RALT", K_RALT},
        {"LCTRL", K_LCTRL},
        {"RCTRL", K_RCTRL},
        {"LSHIFT", K_LSHIFT},
        {"RSHIFT", K_RSHIFT},
        {"MOUSE1", K_MOUSE1},
        {"MOUSE2", K_MOUSE2},
        {"MOUSE3", K_MOUSE3},
        {"MOUSE4", K_MOUSE4},
        {"MOUSE5", K_MOUSE5},
        {"MOUSE6", K_MOUSE6},
        {"MOUSE7", K_MOUSE7},
        {"MOUSE8", K_MOUSE8},
        {"MWHEELDOWN", K_MWHEELDOWN},
        {"MWHEELUP", K_MWHEELUP},
        {"MWHEELRIGHT", K_MWHEELRIGHT},
        {"MWHEELLEFT", K_MWHEELLEFT},
        {"PAD_A", K_PAD_A},
        {"PAD_B", K_PAD_B},
        {"PAD_X", K_PAD_X},
        {"PAD_Y", K_PAD_Y},
        {"PAD_BACK", K_PAD_BACK},
        {"PAD_GUIDE", K_PAD_GUIDE},
        {"PAD_START", K_PAD_START},
        {"PAD_LSTICK", K_PAD_LSTICK},
        {"PAD_RSTICK", K_PAD_RSTICK},
        {"PAD_LSHOULDER", K_PAD_LSHOULDER},
        {"PAD_RSHOULDER", K_PAD_RSHOULDER},
        {"PAD_DPAD_UP", K_PAD_DPAD_UP},
        {"PAD_DPAD_DOWN", K_PAD_DPAD_DOWN},
        {"PAD_DPAD_LEFT", K_PAD_DPAD_LEFT},
        {"PAD_DPAD_RIGHT", K_PAD_DPAD_RIGHT},
        {"PAD_LTRIGGER", K_PAD_LTRIGGER},
        {"PAD_RTRIGGER", K_PAD_RTRIGGER},
        {"PAD_LSTICK_UP", K_PAD_LSTICK_UP},
        {"PAD_LSTICK_DOWN", K_PAD_LSTICK_DOWN},
        {"PAD_LSTICK_LEFT", K_PAD_LSTICK_LEFT},
        {"PAD_LSTICK_RIGHT", K_PAD_LSTICK_RIGHT},
        {"PAD_RSTICK_UP", K_PAD_RSTICK_UP},
        {"PAD_RSTICK_DOWN", K_PAD_RSTICK_DOWN},
        {"PAD_RSTICK_LEFT", K_PAD_RSTICK_LEFT},
        {"PAD_RSTICK_RIGHT", K_PAD_RSTICK_RIGHT},
        {"PAD_PADDLE1", K_PAD_PADDLE1},
        {"PAD_PADDLE2", K_PAD_PADDLE2},
        {"PAD_PADDLE3", K_PAD_PADDLE3},
        {"PAD_PADDLE4", K_PAD_PADDLE4},
        {"PAD_TOUCHPAD", K_PAD_TOUCHPAD},
        {"PAD_MISC1", K_PAD_MISC1},
        {"PAD_MISC2", K_PAD_MISC2},
        {"PAD_MISC3", K_PAD_MISC3},
        {"PAD_MISC4", K_PAD_MISC4},
        {"PAD_MISC5", K_PAD_MISC5},
        {"PAD_MISC6", K_PAD_MISC6},
        {"PAD_MISC7", K_PAD_MISC7},
        {"PAD_MISC8", K_PAD_MISC8},
        {"SEMICOLON", ';'},
    };

    for (const mapping_t &m : key_mappings) {
        if (!Q_stricmp(key_name, m.name)) {
            *keynum = m.keynum;
            return true;
        }
    }

    return false;
}

/*
=============
CGC_KeynumToIcon

Translates a key number into a bind icon path and sizing information.
=============
*/
static bool CGC_KeynumToIcon(int keynum, bind_icon_t &icon)
{
	char pic[MAX_QPATH];

	auto set_keyboard_icon = [&](int value) -> bool {
		Q_snprintf(pic, sizeof(pic), "gfx/controller/keyboard/%d.png", value);
		int w = 0;
		int h = 0;
		cgi.Draw_GetPicSize(&w, &h, pic);
		if (!w || !h) {
			return false;
		}
		icon.name = pic;
		icon.width = w;
		icon.height = h;
		icon.draw_height = BIND_ICON_TARGET_HEIGHT;
		icon.draw_width = (w * BIND_ICON_TARGET_HEIGHT + h / 2) / h;
		if (icon.draw_width <= 0) {
			icon.draw_width = BIND_ICON_TARGET_HEIGHT;
		}
		return true;
	};

	if (keynum >= 0 && keynum < 256 && keynum != '"') {
		if (set_keyboard_icon(keynum)) {
			return true;
		}
	}

	if (keynum >= K_UPARROW && keynum <= K_KP_MULTIPLY) {
		return set_keyboard_icon(keynum + 128);
	}

	if (keynum >= K_LALT && keynum <= K_RSHIFT) {
		return set_keyboard_icon(keynum + 128);
	}

	if (keynum >= K_MOUSEFIRST && keynum <= K_MOUSE8) {
		const int index = keynum - K_MOUSEFIRST;
		Q_snprintf(pic, sizeof(pic), "gfx/controller/mouse/f%04d.png", index);
		int w = 0;
		int h = 0;
		cgi.Draw_GetPicSize(&w, &h, pic);
		if (!w || !h) {
			return false;
		}
		icon.name = pic;
		icon.width = w;
		icon.height = h;
		icon.draw_height = BIND_ICON_TARGET_HEIGHT;
		icon.draw_width = (w * BIND_ICON_TARGET_HEIGHT + h / 2) / h;
		if (icon.draw_width <= 0) {
			icon.draw_width = BIND_ICON_TARGET_HEIGHT;
		}
		return true;
	}

	if (keynum == K_MWHEELUP || keynum == K_MWHEELDOWN) {
		const int index = (keynum == K_MWHEELUP) ? 8 : 9;
		Q_snprintf(pic, sizeof(pic), "gfx/controller/mouse/f%04d.png", index);
		int w = 0;
		int h = 0;
		cgi.Draw_GetPicSize(&w, &h, pic);
		if (!w || !h) {
			return false;
		}
		icon.name = pic;
		icon.width = w;
		icon.height = h;
		icon.draw_height = BIND_ICON_TARGET_HEIGHT;
		icon.draw_width = (w * BIND_ICON_TARGET_HEIGHT + h / 2) / h;
		if (icon.draw_width <= 0) {
			icon.draw_width = BIND_ICON_TARGET_HEIGHT;
		}
		return true;
	}

	if (keynum >= K_PAD_FIRST && keynum <= K_PAD_FIRST + 0x22) {
		const int index = keynum - K_PAD_FIRST;
		Q_snprintf(pic, sizeof(pic), "gfx/controller/generic/f%04x.png", index);
		int w = 0;
		int h = 0;
		cgi.Draw_GetPicSize(&w, &h, pic);
		if (!w || !h) {
			return false;
		}
		icon.name = pic;
		icon.width = w;
		icon.height = h;
		icon.draw_height = BIND_ICON_TARGET_HEIGHT;
		icon.draw_width = (w * BIND_ICON_TARGET_HEIGHT + h / 2) / h;
		if (icon.draw_width <= 0) {
			icon.draw_width = BIND_ICON_TARGET_HEIGHT;
		}
		return true;
	}

	return false;
}

/*
=============
CGC_BuildBindIcon

Attempts to resolve a binding into a controller icon path, falling back to formatted text.
=============
*/
static bool CGC_BuildBindIcon(const char *binding, bind_icon_t &icon, std::string &fallback)
{
	fallback.clear();
	if (!binding || !*binding) {
		fallback.assign("UNBOUND");
		return false;
	}

	const char *key_name = cgi.CL_GetKeyBinding(binding);
	if (!key_name || !*key_name) {
		fallback.assign("UNBOUND");
		return false;
	}

	int keynum;
	if (!CGC_KeyNameToKeynum(key_name, &keynum)) {
		fallback = CGC_FormatKeyName(key_name);
		return false;
	}

	if (CGC_KeynumToIcon(keynum, icon)) {
		return true;
	}

	fallback = CGC_FormatKeyName(key_name);
	return false;
}

/*
=============
CGC_BuildBindSegments

Parses a centerprint line with :bind: markers into text and icon segments and calculates layout metrics.
=============
*/
static void CGC_BuildBindSegments(const char *line, size_t line_limit,
			std::vector<bind_segment_t> &segments,
			int &line_width, int &line_height)
{
	segments.clear();
	std::string text_buffer;

	const char *cursor = line;
	const char *line_end = line + line_limit;

	auto flush_text = [&]() {
		if (text_buffer.empty()) {
			return;
		}
		if (!segments.empty() && segments.back().type == bind_segment_type_t::text) {
			segments.back().text.append(text_buffer);
		} else {
			bind_segment_t seg;
			seg.type = bind_segment_type_t::text;
			seg.text = text_buffer;
			segments.push_back(std::move(seg));
		}
		text_buffer.clear();
	};

	while (cursor < line_end) {
		if ((line_end - cursor) >= 6 && !strncmp(cursor, ":bind:", 6)) {
			const char *command_start = cursor + 6;
			const char *command_end = command_start;
			while (command_end < line_end && *command_end != ':') {
				++command_end;
			}

			if (command_end < line_end && *command_end == ':') {
				flush_text();
				std::string command(command_start, command_end - command_start);
				bind_icon_t icon;
				std::string fallback;
				bool add_space = false;
				if (CGC_BuildBindIcon(command.c_str(), icon, fallback)) {
					bind_segment_t seg;
					seg.type = bind_segment_type_t::icon;
					seg.icon = std::move(icon);
					segments.push_back(std::move(seg));
					add_space = true;
				} else if (!fallback.empty()) {
					text_buffer.append(fallback);
				}
				cursor = command_end + 1;
				if (add_space && cursor < line_end && *cursor != ' ' && *cursor != '\t') {
					text_buffer.push_back(' ');
				}
				continue;
			}
		}

		text_buffer.push_back(*cursor);
		++cursor;
	}

	flush_text();

	line_width = 0;
	line_height = CONCHAR_HEIGHT;

	for (const bind_segment_t &seg : segments) {
		if (seg.type == bind_segment_type_t::text) {
			line_width += static_cast<int>(seg.text.length()) * CONCHAR_WIDTH;
		} else {
			line_width += seg.icon.draw_width;
			if (seg.icon.draw_height > line_height) {
				line_height = seg.icon.draw_height;
			}
		}
	}
}

struct hud_space_t
{
    const vrect_t &left;
    const vrect_t &center;
    const vrect_t &right;
};

static inline hud_space_t CGC_GetHudSpace()
{
    return {
        *cgi.SCR_GetVirtualScreen(text_align_t::LEFT),
        *cgi.SCR_GetVirtualScreen(text_align_t::CENTER),
        *cgi.SCR_GetVirtualScreen(text_align_t::RIGHT)
    };
}

static inline int CGC_LeftX(const hud_space_t &space, int base_x)
{
    return space.left.x + base_x;
}

static inline int CGC_RightX(const hud_space_t &space, int base_x)
{
    return space.right.x + space.right.width + base_x;
}

static inline int CGC_CenterX(const hud_space_t &space, int base_x, int half_width)
{
    return space.center.x + space.center.width / 2 + (base_x - half_width);
}

} // namespace
#define UI_DROPSHADOW       BIT(4)
#define UI_XORCOLOR         BIT(7)

float SCR_FadeAlpha(unsigned startTime, unsigned visTime, unsigned fadeTime);
bool SCR_ParseColor(const char *s, color_t *color);

// ==========================================================================

static cvar_t   *scr_centertime;
static cvar_t   *scr_draw2d;

static cvar_t   *ch_scale;
static cvar_t   *ch_x;
static cvar_t   *ch_y;

static char     scr_centerstring[MAX_STRING_CHARS];
static unsigned scr_centertime_start;   // for slow victory printing
static int      scr_center_lines;

static color_t  ui_color;

static void CGC_Init(void)
{
    /* We don't consider rerelease servers here and assume the appropriate
     * cgame is used in that case */
    csr = cgix.IsExtendedServer() ? &cs_remap_q2pro_new : &cs_remap_old;
    max_stats = cgix.GetMaxStats();

    scr_centertime = cgi.cvar("scr_centertime", "2.5", 0);
    scr_draw2d = cgi.cvar("scr_draw2d", "2", 0);

    ch_scale = cgi.cvar("ch_scale", "1", 0);
    ch_x = cgi.cvar("ch_x", "0", 0);
    ch_y = cgi.cvar("ch_y", "0", 0);

    ui_color = COLOR_WHITE;
}

static void CGC_Shutdown(void)
{
    scr_draw2d = NULL;
    ch_scale = NULL;
    ch_x = NULL;
    ch_y = NULL;
}

static void DrawPic(int x, int y, const char* pic)
{
    int w = 0, h = 0;
    cgi.Draw_GetPicSize(&w, &h, pic);
    if (!w || !h)
        return;
    cgi.SCR_DrawPic(x, y, w, h, pic);
}

static void CG_DrawString(int x, int y, int flags, size_t maxlen, const char *s, color_t color)
{
    while (maxlen-- && *s) {
        byte c = *s++;
        cgix.DrawCharEx(x, y, flags, c, color);
        x += CONCHAR_WIDTH;
    }
}

/*
==============
DrawStringEx
==============
*/
static void CG_DrawStringEx(int x, int y, int flags, size_t maxlen, const char *s, color_t color)
{
    size_t len = strlen(s);

    if (len > maxlen) {
        len = maxlen;
    }

    if ((flags & UI_CENTER) == UI_CENTER) {
        x -= len * CONCHAR_WIDTH / 2;
    } else if (flags & UI_RIGHT) {
        x -= len * CONCHAR_WIDTH;
    }

    CG_DrawString(x, y, flags, maxlen, s, color);
}

/*
==============
DrawStringMulti
==============
*/
static void CG_DrawStringMulti(int x, int y, int flags, size_t maxlen, const char *s, color_t color)
{
    if (!s || !maxlen) {
        return;
    }

    std::vector<bind_segment_t> segments;

    while (*s) {
        const char *p = strchr(s, '\n');
        size_t len = p ? static_cast<size_t>(p - s) : strlen(s);
        if (len > maxlen) {
            len = maxlen;
        }

        int line_width = 0;
        int line_height = CONCHAR_HEIGHT;
        CGC_BuildBindSegments(s, len, segments, line_width, line_height);

        int draw_x = x;
        if ((flags & UI_CENTER) == UI_CENTER) {
            draw_x -= line_width / 2;
        } else if (flags & UI_RIGHT) {
            draw_x -= line_width;
        }

        const int text_flags = flags & ~(UI_CENTER | UI_RIGHT);
        const int text_y = y + (line_height - CONCHAR_HEIGHT) / 2;

        for (const bind_segment_t &seg : segments) {
            if (seg.type == bind_segment_type_t::text) {
                if (!seg.text.empty()) {
                    CG_DrawString(draw_x, text_y, text_flags,
                                  seg.text.length(), seg.text.c_str(), color);
                    draw_x += static_cast<int>(seg.text.length()) * CONCHAR_WIDTH;
                }
            } else {
                const int icon_y = y + (line_height - seg.icon.draw_height) / 2;
                cgi.SCR_DrawPic(draw_x, icon_y, seg.icon.draw_width,
                                seg.icon.draw_height, seg.icon.name.c_str());
                draw_x += seg.icon.draw_width;
            }
        }

        y += line_height;

        if (!p) {
            break;
        }
        s = p + 1;
    }
}

#define HUD_DrawString(x, y, string) \
    CG_DrawString(x, y, 0, MAX_STRING_CHARS, string, ui_color)

#define HUD_DrawAltString(x, y, string) \
    CG_DrawString(x, y, UI_XORCOLOR, MAX_STRING_CHARS, string, ui_color)

#define HUD_DrawCenterString(x, y, string) \
    CG_DrawStringMulti(x, y, UI_CENTER, MAX_STRING_CHARS, string, ui_color)

#define HUD_DrawAltCenterString(x, y, string) \
    CG_DrawStringMulti(x, y, UI_CENTER | UI_XORCOLOR, MAX_STRING_CHARS, string, ui_color)

#define HUD_DrawRightString(x, y, string) \
    CG_DrawStringEx(x, y, UI_RIGHT, MAX_STRING_CHARS, string, ui_color)

#define HUD_DrawAltRightString(x, y, string) \
    CG_DrawStringEx(x, y, UI_RIGHT | UI_XORCOLOR, MAX_STRING_CHARS, string, ui_color)

static const char field_pic[] = "field_3";
static const char inven_pic[] = "inventory";

#define DIGIT_WIDTH 16
#define STAT_PICS       11
#define STAT_MINUS      (STAT_PICS - 1)  // num frame for '-' stats digit

static const char *const sb_nums[2][STAT_PICS] = {
    {
        "num_0", "num_1", "num_2", "num_3", "num_4", "num_5",
        "num_6", "num_7", "num_8", "num_9", "num_minus"
    },
    {
        "anum_0", "anum_1", "anum_2", "anum_3", "anum_4", "anum_5",
        "anum_6", "anum_7", "anum_8", "anum_9", "anum_minus"
    }
};

static void HUD_DrawNumber(int x, int y, int color, int width, int value)
{
    char    num[16], *ptr;
    int     l;
    int     frame;

    if (width < 1)
        return;

    // draw number string
    if (width > 5)
        width = 5;

    color &= 1;

    l = Q_scnprintf(num, sizeof(num), "%i", value);
    if (l > width)
        l = width;
    x += 2 + DIGIT_WIDTH * (width - l);

    ptr = num;
    while (*ptr && l) {
        if (*ptr == '-')
            frame = STAT_MINUS;
        else
            frame = *ptr - '0';

        DrawPic(x, y, sb_nums[color][frame]);
        x += DIGIT_WIDTH;
        ptr++;
        l--;
    }
}

static void SCR_SkipToEndif(const char **s)
{
    int i, skip = 1;
    char *token;

    while (*s) {
        token = COM_Parse(s);
        if (!strcmp(token, "xl") || !strcmp(token, "xr") || !strcmp(token, "xv") ||
            !strcmp(token, "yt") || !strcmp(token, "yb") || !strcmp(token, "yv") ||
            !strcmp(token, "pic") || !strcmp(token, "picn") || !strcmp(token, "color") ||
            strstr(token, "string")) {
            COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "client")) {
            for (i = 0; i < 6; i++)
                COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "ctf")) {
            for (i = 0; i < 5; i++)
                COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "num") || !strcmp(token, "health_bars")) {
            COM_SkipToken(s);
            COM_SkipToken(s);
            continue;
        }

        if (!strcmp(token, "hnum")) continue;
        if (!strcmp(token, "anum")) continue;
        if (!strcmp(token, "rnum")) continue;

        if (!strcmp(token, "if")) {
            COM_SkipToken(s);
            skip++;
            continue;
        }

        if (!strcmp(token, "endif")) {
            if (--skip > 0)
                continue;
            return;
        }
    }
}

static inline int flash_frame(void)
{
    /* Original logic:
     * ((cl.frame.number / CL_FRAMEDIV) >> 2) & 1
     * Which in vanilla works out to a flash lasting 4 frames, or 400 ms */
    return (cgi.CL_ClientTime() / 400) & 1;
}

static void layout_pic(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // draw a pic from a stat number
    char* token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= max_stats) {
        cgi.Com_Error(va("%s: invalid stat index", __func__));
    }
    value = ps->stats[value];
    if (value < 0 || value >= csr->max_images) {
        cgi.Com_Error(va("%s: invalid pic index", __func__));
    }
    const char* pic = cgi.get_configstring(csr->images + value);
    if (pic[0]) {
        // hack for action mod scope scaling
        if (x == hud_vrect.width  / 2 - 160 &&
            y == hud_vrect.height / 2 - 120 &&
            Com_WildCmp("scope?x", pic))
        {
            int w = 320 * ch_scale->value;
            int h = 240 * ch_scale->value;
            cgi.SCR_DrawPic((hud_vrect.width  - w) / 2 + ch_x->integer,
                            (hud_vrect.height - h) / 2 + ch_y->integer,
                            w, h, pic);
        } else {
            DrawPic(x, y, pic);
        }
    }
}

static void layout_client(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // draw a deathmatch client block
    char    buffer[MAX_QPATH];
    int     score, ping, time;

    const hud_space_t hud_space = CGC_GetHudSpace();
    char* token = COM_Parse(s);
    x = CGC_CenterX(hud_space, atoi(token), 160);
    token = COM_Parse(s);
    y = hud_vrect.y + hud_vrect.height / 2 - 120 + atoi(token);

    token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= MAX_CLIENTS) {
        cgi.Com_Error(va("%s: invalid client index", __func__));
    }

    token = COM_Parse(s);
    score = atoi(token);

    token = COM_Parse(s);
    ping = atoi(token);

    token = COM_Parse(s);
    time = atoi(token);

    HUD_DrawAltString(x + 32, y, cgi.CL_GetClientName(value));
    HUD_DrawString(x + 32, y + CONCHAR_HEIGHT, "Score: ");
    Q_snprintf(buffer, sizeof(buffer), "%i", score);
    HUD_DrawAltString(x + 32 + 7 * CONCHAR_WIDTH, y + CONCHAR_HEIGHT, buffer);
    Q_snprintf(buffer, sizeof(buffer), "Ping:  %i", ping);
    HUD_DrawString(x + 32, y + 2 * CONCHAR_HEIGHT, buffer);
    Q_snprintf(buffer, sizeof(buffer), "Time:  %i", time);
    HUD_DrawString(x + 32, y + 3 * CONCHAR_HEIGHT, buffer);

    DrawPic(x, y, cgi.CL_GetClientPic(value));
}

static void layout_ctf(vrect_t hud_vrect, const char **s, int32_t playernum, const player_state_t *ps, int x, int y)
{
    // draw a ctf client block
    char    buffer[MAX_QPATH];
    int     score, ping;

    const hud_space_t hud_space = CGC_GetHudSpace();
    char* token = COM_Parse(s);
    x = CGC_CenterX(hud_space, atoi(token), 160);
    token = COM_Parse(s);
    y = hud_vrect.y + hud_vrect.height / 2 - 120 + atoi(token);

    token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= MAX_CLIENTS) {
        cgi.Com_Error(va("%s: invalid client index", __func__));
    }

    token = COM_Parse(s);
    score = atoi(token);

    token = COM_Parse(s);
    ping = atoi(token);
    if (ping > 999)
        ping = 999;

    Q_snprintf(buffer, sizeof(buffer), "%3d %3d %-12.12s",
                score, ping, cgi.CL_GetClientName(value));
    if (value == playernum) {
        HUD_DrawAltString(x, y, buffer);
    } else {
        HUD_DrawString(x, y, buffer);
    }
}

static void layout_num(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // draw a number
    char* token = COM_Parse(s);
    int width = atoi(token);
    token = COM_Parse(s);
    int value = atoi(token);
    if (value < 0 || value >= MAX_STATS) {
        cgi.Com_Error(va("%s: invalid stat index", __func__));
    }
    value = ps->stats[value];
    HUD_DrawNumber(x, y, 0, width, value);
}

static void layout_hnum(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // health number
    int     color;

    int width = 3;
    int value = ps->stats[STAT_HEALTH];
    if (value > 25)
        color = 0;  // green
    else if (value > 0)
        color = flash_frame();     // flash
    else
        color = 1;

    if (ps->stats[STAT_FLASHES] & 1)
        DrawPic(x, y, field_pic);

    HUD_DrawNumber(x, y, color, width, value);
}

static void layout_anum(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // ammo number
    int     color;

    int width = 3;
    int value = ps->stats[STAT_AMMO];
    if (value > 5)
        color = 0;  // green
    else if (value >= 0)
        color = flash_frame();     // flash
    else
        return;   // negative number = don't show

    if (ps->stats[STAT_FLASHES] & 4)
        DrawPic(x, y, field_pic);

    HUD_DrawNumber(x, y, color, width, value);
}

static void layout_rnum(vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    // armor number
    int     color;

    int width = 3;
    int value = ps->stats[STAT_ARMOR];
    if (value < 1)
        return;

    color = 0;  // green

    if (ps->stats[STAT_FLASHES] & 2)
        DrawPic(x, y, field_pic);

    HUD_DrawNumber(x, y, color, width, value);
}

static void layout_stat(const char* token, vrect_t hud_vrect, const char **s, const player_state_t *ps, int x, int y)
{
    bool localize = !strncmp(token, "loc_", 4);
    const char *cmd = token + 5 + (localize ? 4 : 0);
    token = COM_Parse(s);
    int index = atoi(token);
    if (index < 0 || index >= MAX_STATS) {
        cgi.Com_Error(va("%s: invalid stat index", __func__));
    }
    index = ps->stats[index];
    if (index < 0 || index >= csr->end) {
        cgi.Com_Error(va("%s: invalid string index", __func__));
    }
    const char* str = cgi.get_configstring(index);
    if (localize)
        str = cgi.Localize(str, NULL, 0);
    if (!strcmp(cmd, "string"))
        HUD_DrawString(x, y, str);
    else if (!strcmp(cmd, "string2"))
        HUD_DrawAltString(x, y, str);
    else if (!strcmp(cmd, "cstring"))
        HUD_DrawCenterString(x + 320 / 2, y, str);
    else if (!strcmp(cmd, "cstring2"))
        HUD_DrawAltCenterString(x + 320 / 2, y, str);
    else if (!strcmp(cmd, "rstring"))
        HUD_DrawRightString(x, y, str);
    else if (!strcmp(cmd, "rstring2"))
        HUD_DrawAltRightString(x, y, str);
}

static char arg_tokens[MAX_LOCALIZATION_ARGS + 1][MAX_TOKEN_CHARS];
static const char *arg_buffers[MAX_LOCALIZATION_ARGS];

static const char *parse_loc_string(const char** s)
{
    int num_args = atoi(COM_Parse (s));

    if (num_args < 0 || num_args >= MAX_LOCALIZATION_ARGS)
        cgi.Com_Error(va("%s: Bad loc string", __func__));

    // parse base
    char* token = COM_Parse (s);
    Q_strlcpy(arg_tokens[0], token, sizeof(arg_tokens[0]));

    // parse args
    for (int i = 0; i < num_args; i++)
    {
        token = COM_Parse (s);
        Q_strlcpy(arg_tokens[1 + i], token, sizeof(arg_tokens[0]));
        arg_buffers[i] = arg_tokens[1 + i];
    }

    return cgi.Localize(arg_tokens[0], arg_buffers, num_args);
}

static void SCR_DrawHealthBar(vrect_t hud_vrect, int x, int y, int value)
{
    if (!value)
        return;

    const rgba_t rgba_fg{ COLOR_U32_RGBA(239, 0, 0, 255) };    // index 240
    const rgba_t rgba_bg{ COLOR_U32_RGBA(63, 63, 63, 255) };    // index 4

    int bar_width = hud_vrect.width / 3;
    float percent = (value - 1) / 254.0f;
    int w = bar_width * percent + 0.5f;
    int h = CONCHAR_HEIGHT / 2;

    x -= bar_width / 2;
    cgi.SCR_DrawColorPic(x, y, w, h, "_white", &rgba_fg);
    cgi.SCR_DrawColorPic(x + w, y, bar_width -w, h, "_white", &rgba_bg);
}

static void SCR_ExecuteLayoutString(vrect_t hud_vrect, const char *s, int32_t playernum, const player_state_t *ps)
{
    int     x, y;
    int     value;
    char    *token;

    if (!s[0])
        return;

    x = 0;
    y = 0;

    const hud_space_t hud_space = CGC_GetHudSpace();

    while (s) {
        token = COM_Parse(&s);
        if (token[2] == 0) {
            if (token[0] == 'x') {
                if (token[1] == 'l') {
                    token = COM_Parse(&s);
                    x = CGC_LeftX(hud_space, atoi(token));
                    continue;
                }

                if (token[1] == 'r') {
                    token = COM_Parse(&s);
                    x = CGC_RightX(hud_space, atoi(token));
                    continue;
                }

                if (token[1] == 'v') {
                    token = COM_Parse(&s);
                    x = CGC_CenterX(hud_space, atoi(token), 160);
                    continue;
                }
            }

            if (token[0] == 'y') {
                if (token[1] == 't') {
                    token = COM_Parse(&s);
                    y = hud_vrect.y + atoi(token);
                    continue;
                }

                if (token[1] == 'b') {
                    token = COM_Parse(&s);
                    y = hud_vrect.y + hud_vrect.height + atoi(token);
                    continue;
                }

                if (token[1] == 'v') {
                    token = COM_Parse(&s);
                    y = hud_vrect.y + hud_vrect.height / 2 - 120 + atoi(token);
                    continue;
                }
            }
        }

        if (!strcmp(token, "pic")) {
            layout_pic(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "client")) {
            layout_client(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "ctf")) {
            layout_ctf(hud_vrect, &s, playernum, ps, x, y);
            continue;
        }

        if (!strcmp(token, "picn")) {
            // draw a pic from a name
            token = COM_Parse(&s);
            DrawPic(x, y, token);
            continue;
        }

        if (!strcmp(token, "num")) {
            layout_num(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "hnum")) {
            layout_hnum(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "anum")) {
            layout_anum(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "rnum")) {
            layout_rnum(hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strncmp(token, "stat_", 5) || !strncmp(token, "loc_stat_", 9)) {
            layout_stat(token, hud_vrect, &s, ps, x, y);
            continue;
        }

        if (!strcmp(token, "cstring") || !strcmp(token, "loc_cstring")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawCenterString(x + 320 / 2, y, str);
            continue;
        }

        if (!strcmp(token, "cstring2") || !strcmp(token, "loc_cstring2")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawAltCenterString(x + 320 / 2, y, str);
            continue;
        }

        if (!strcmp(token, "string") || !strcmp(token, "loc_string")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawString(x, y, str);
            continue;
        }

        if (!strcmp(token, "string2") || !strcmp(token, "string2")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawAltString(x, y, str);
            continue;
        }

        if (!strcmp(token, "rstring") || !strcmp(token, "loc_rstring")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawRightString(x, y, str);
            continue;
        }

        if (!strcmp(token, "rstring2") || !strcmp(token, "loc_rstring2")) {
            bool localize = !strncmp(token, "loc_", 4);
            const char* str = localize ? parse_loc_string(&s) : COM_Parse(&s);
            HUD_DrawAltRightString(x, y, str);
            continue;
        }

        if (!strcmp(token, "if")) {
            token = COM_Parse(&s);
            value = atoi(token);
            if (value < 0 || value >= MAX_STATS) {
                cgi.Com_Error(va("%s: invalid stat index", __func__));
            }
            value = ps->stats[value];
            if (!value) {   // skip to endif
                if (cgix.IsExtendedServer()) {
                    SCR_SkipToEndif(&s);
                } else while (strcmp(token, "endif")) {
                    token = COM_Parse(&s);
                    if (!s) {
                        break;
                    }
                }
            }
            continue;
        }

        // Q2PRO extension
        if (!strcmp(token, "color")) {
            color_t     color;

            token = COM_Parse(&s);
            if (SCR_ParseColor(token, &color)) {
                ui_color = color;
            }
            continue;
        }

        if (!strcmp(token, "health_bars")) {
            token = COM_Parse(&s);
            value = Q_atoi(token);
            if (value < 0 || value >= MAX_STATS) {
                Com_Error(ERR_DROP, "%s: invalid stat index", __func__);
            }
            value = ps->stats[value];

            token = COM_Parse(&s);
            int index = Q_atoi(token);
            if (index < 0 || index >= csr->end) {
                Com_Error(ERR_DROP, "%s: invalid string index", __func__);
            }

            HUD_DrawCenterString(x + 320 / 2, y, cgi.get_configstring(index));
            SCR_DrawHealthBar(hud_vrect, x + 320 / 2, y + CONCHAR_HEIGHT + 4, value & 0xff);
            SCR_DrawHealthBar(hud_vrect, x + 320 / 2, y + CONCHAR_HEIGHT + 12, (value >> 8) & 0xff);
            continue;
        }
    }

    ui_color = COLOR_WHITE;
}

// The status bar is a small layout program that is based on the stats array
static void draw_stats(vrect_t hud_vrect, int32_t playernum, const player_state_t *ps)
{
    if (scr_draw2d->integer <= 1)
        return;
    if (ps->stats[STAT_LAYOUTS] & LAYOUTS_HIDE_HUD)
        return;

    SCR_ExecuteLayoutString(hud_vrect, cgi.get_configstring(CS_STATUSBAR), playernum, ps);
}

static void SCR_DrawLayout(vrect_t hud_vrect, const cg_server_data_t *data, int32_t playernum, const player_state_t *ps)
{
    if (scr_draw2d->integer == 3 /*&& !Key_IsDown(K_F1)*/)
        return;     // turn off for GTV

    /*if (cls.demo.playback && Key_IsDown(K_F1))
        goto draw;*/

    if (!(ps->stats[STAT_LAYOUTS] & LAYOUTS_LAYOUT))
        return;

//draw:
    SCR_ExecuteLayoutString(hud_vrect, data->layout, playernum, ps);
}

#define DISPLAY_ITEMS   17

static void SCR_DrawInventory(vrect_t hud_vrect, const cg_server_data_t *data, const player_state_t *ps)
{
    int     i;
    int     num, selected_num, item;
    int     index[MAX_ITEMS];
    char    string[MAX_STRING_CHARS];
    int     x, y;
    const char  *bind;
    int     selected;
    int     top;

    if (!(ps->stats[STAT_LAYOUTS] & LAYOUTS_INVENTORY))
        return;

    selected = ps->stats[STAT_SELECTED_ITEM];

    num = 0;
    selected_num = 0;
    for (i = 0; i < MAX_ITEMS; i++) {
        if (i == selected) {
            selected_num = num;
        }
        if (data->inventory[i]) {
            index[num++] = i;
        }
    }

    // determine scroll point
    top = selected_num - DISPLAY_ITEMS / 2;
    if (top > num - DISPLAY_ITEMS) {
        top = num - DISPLAY_ITEMS;
    }
    if (top < 0) {
        top = 0;
    }

    x = hud_vrect.x + (hud_vrect.width - 256) / 2;
    y = hud_vrect.y + (hud_vrect.height - 240) / 2;

    DrawPic(x, y + 8, inven_pic);
    y += 24;
    x += 24;

    HUD_DrawString(x, y, "hotkey ### item");
    y += CONCHAR_HEIGHT;

    HUD_DrawString(x, y, "------ --- ----");
    y += CONCHAR_HEIGHT;

    for (i = top; i < num && i < top + DISPLAY_ITEMS; i++) {
        item = index[i];
        // search for a binding
        Q_concat(string, sizeof(string), "use ", cgi.get_configstring(csr->items + item));
        bind = cgi.CL_GetKeyBinding(string);

        Q_snprintf(string, sizeof(string), "%6s %3i %s",
                   bind, data->inventory[item],
                   cgi.Localize(cgi.get_configstring(csr->items + item), NULL, 0));

        if (item != selected) {
            HUD_DrawAltString(x, y, string);
        } else {    // draw a blinky cursor by the selected item
            HUD_DrawString(x, y, string);
            if ((cgi.CL_ClientRealTime() >> 8) & 1) {
                cgi.SCR_DrawChar(x - CONCHAR_WIDTH, y, 1, 15, false);
            }
        }

        y += CONCHAR_HEIGHT;
    }
}

static void SCR_DrawCenterString(vrect_t hud_vrect)
{
    int y;
    float alpha;

    Cvar_ClampValue(scr_centertime, 0.3f, 10.0f);

    alpha = SCR_FadeAlpha(scr_centertime_start, scr_centertime->value * 1000, 300);
    if (!alpha) {
        return;
    }

    color_t color = ui_color;
    color.a *= alpha;

    y = hud_vrect.height / 4 - scr_center_lines * 8 / 2;

    CG_DrawStringMulti(hud_vrect.width / 2, y, UI_CENTER,
                       MAX_STRING_CHARS, scr_centerstring, color);
}

static void CGC_DrawHUD (int32_t isplit, const cg_server_data_t *data, vrect_t hud_vrect, vrect_t hud_safe, int32_t scale, int32_t playernum, const player_state_t *ps)
{
    // Note: isplit is ignored, due to missing split screen support

    draw_stats(hud_vrect, playernum, ps);

    SCR_DrawLayout(hud_vrect, data, playernum, ps);

    SCR_DrawInventory(hud_vrect, data, ps);

    SCR_DrawCenterString(hud_vrect);
}

static void CGC_TouchPics(void)
{
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < STAT_PICS; j++)
            cgi.Draw_RegisterPic(sb_nums[i][j]);
    }

    cgi.Draw_RegisterPic(field_pic);
    cgi.Draw_RegisterPic(inven_pic);
}

static uint32_t CGC_GetOwnedWeaponWheelWeapons(const player_state_t *ps)
{
    return 0;
}

static int16_t CGC_GetHitMarkerDamage(const player_state_t *ps)
{
    return 0;
}

static void CGC_Pmove(pmove_t *pmove)
{
    Pmove(pmove, cgix.GetPmoveParams());
}

static void CGC_ParseConfigString(int32_t i, const char *s) {}

static void CGC_ParseCenterPrint(const char *str, int isplit, bool instant)
{
    const char  *s;

    scr_centertime_start = cgi.CL_ClientRealTime();
    if (!strcmp(scr_centerstring, str)) {
        return;
    }

    Q_strlcpy(scr_centerstring, str, sizeof(scr_centerstring));

    // count the number of lines for centering
    scr_center_lines = 1;
    s = str;
    while (*s) {
        if (*s == '\n')
            scr_center_lines++;
        s++;
    }

    // echo it to the console
    cgi.Com_Print(va("%s\n", scr_centerstring));
    // Con_ClearNotify_f();
}

static void CGC_ClearCenterprint(int32_t isplit)
{
    scr_centerstring[0] = 0;
}

static void CGC_NotifyMessage(int32_t isplit, const char *msg, bool is_chat)
{
    Con_SkipNotify(false);
    if (is_chat)
        Con_SetColor(COLOR_INDEX_ALT);
    Con_Print(msg);
    if (is_chat)
        Con_SetColor(COLOR_INDEX_NONE);
    Con_SkipNotify(true);
}

const char cgame_q2pro_extended_support_ext[] = "q2pro:extended_support";

cgame_export_t cgame_classic{
    CGAME_API_VERSION,

    CGC_Init,
    CGC_Shutdown,

    CGC_DrawHUD,
    CGC_TouchPics,

    nullptr, // LayoutFlags
    nullptr, // GetActiveWeaponWheelWeapon
    CGC_GetOwnedWeaponWheelWeapons,
    nullptr, // GetWeaponWheelAmmoCount
    nullptr, // GetPowerupWheelCount
    CGC_GetHitMarkerDamage,

    CGC_Pmove,

    CGC_ParseConfigString,

    CGC_ParseCenterPrint,
    nullptr, // ClearNotify
    CGC_ClearCenterprint,
    CGC_NotifyMessage,
    nullptr, // GetMonsterFlashOffset
    nullptr  // GetExtension
};

cgame_export_t *GetClassicCGameAPI(cgame_import_t *import)
{
    cgi = *import;
    cgix = *((cgame_q2pro_extended_support_ext_t*)import->GetExtension(cgame_q2pro_extended_support_ext));
    return &cgame_classic;
}
