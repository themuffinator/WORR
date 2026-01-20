#include "ui/ui_internal.h"

namespace {

typedef struct {
    qhandle_t pic;
    int w;
    int h;
    bool tried;
} bind_icon_cache_t;

static bind_icon_cache_t ui_bind_icon_cache[256];

static int UI_MapKeynumToMouseIcon(int keynum)
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

static int UI_MapKeynumToKeyboardIcon(int keynum)
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

static bool UI_BuildGamepadIconPath(int keynum, char *out_path, size_t out_size)
{
    if (keynum < K_GAMEPAD_FIRST || keynum > K_GAMEPAD_LAST)
        return false;

    int index = keynum - K_GAMEPAD_FIRST;
    if (index < 0)
        return false;
    Q_snprintf(out_path, out_size, "/gfx/controller/generic/f%04x.png", index);
    return true;
}

static bool UI_BuildBindIconPath(int keynum, char *out_path, size_t out_size)
{
    int mouse_icon = UI_MapKeynumToMouseIcon(keynum);
    if (mouse_icon >= 0) {
        Q_snprintf(out_path, out_size, "/gfx/controller/mouse/f000%i.png", mouse_icon);
        return true;
    }

    if (UI_BuildGamepadIconPath(keynum, out_path, out_size)) {
        return true;
    }

    int key_icon = UI_MapKeynumToKeyboardIcon(keynum);
    if (key_icon >= 0) {
        Q_snprintf(out_path, out_size, "/gfx/controller/keyboard/%i.png", key_icon);
        return true;
    }

    return false;
}

static bool UI_GetBindIconForKey(int keynum, qhandle_t *pic, int *w, int *h)
{
    if (keynum < 0 || keynum > 255)
        return false;

    bind_icon_cache_t *entry = &ui_bind_icon_cache[keynum];
    if (!entry->tried) {
        entry->tried = true;
        char path[MAX_QPATH];

        if (UI_BuildBindIconPath(keynum, path, sizeof(path))) {
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

} // namespace

namespace ui {

namespace {

constexpr int kHintPadding = CONCHAR_WIDTH / 2;
constexpr int kHintGap = CONCHAR_WIDTH;

const char *ShortKeyLabel(int key, const char *fallback)
{
    if (fallback && *fallback)
        return fallback;
    if (key < 0)
        return "???";

    const char *label = Key_KeynumToString(key);
    if (!label || !*label)
        return "???";

    if (!Q_stricmp(label, "ESCAPE"))
        return "Esc";
    if (!Q_stricmp(label, "ENTER"))
        return "Enter";
    if (!Q_stricmp(label, "BACKSPACE"))
        return "Bksp";
    if (!Q_stricmp(label, "TAB"))
        return "Tab";
    if (!Q_stricmp(label, "SPACE"))
        return "Space";
    if (!Q_stricmp(label, "DEL"))
        return "Del";

    return label;
}

int KeyCapWidth(int height, const char *label)
{
    int text_width = TextWidth(label);
    int width = text_width + kHintPadding * 2;
    if (width < height)
        width = height;
    return width;
}

int DrawKeyCap(int x, int y, int height, color_t color, const char *label)
{
    int width = KeyCapWidth(height, label);
    color_t fill = COLOR_SETA_U8(color, 80);
    color_t border = COLOR_SETA_U8(color, 160);

    R_DrawFill32(x, y, width, height, fill);
    R_DrawFill32(x, y, width, 1, border);
    R_DrawFill32(x, y + height - 1, width, 1, border);
    R_DrawFill32(x, y, 1, height, border);
    R_DrawFill32(x + width - 1, y, 1, height, border);

    int text_x = x + (width - TextWidth(label)) / 2;
    int text_y = y + (height - CONCHAR_HEIGHT) / 2;
    UI_DrawString(text_x, text_y, UI_LEFT, color, label);
    return width;
}

int DrawHintItem(int x, int y, int height, const UiHint &hint, color_t key_color, color_t text_color)
{
    const char *key_label = hint.keyLabel.empty() ? nullptr : hint.keyLabel.c_str();
    int key_w = UI_DrawKeyIcon(x, y, height, key_color, hint.key, key_label);

    int text_x = x + key_w + kHintPadding;
    int text_y = y + (height - CONCHAR_HEIGHT) / 2;
    UI_DrawString(text_x, text_y, UI_LEFT, text_color, hint.label.c_str());

    return key_w + kHintPadding + TextWidth(hint.label);
}

} // namespace

int UI_DrawKeyIcon(int x, int y, int height, color_t color, int keynum, const char *label)
{
    if (keynum >= 0) {
        qhandle_t pic;
        int w = 0;
        int h = 0;
        if (UI_GetBindIconForKey(keynum, &pic, &w, &h) && w > 0 && h > 0) {
            float scale = height > 0 ? (static_cast<float>(height) / static_cast<float>(h)) : 1.0f;
            int draw_w = max(1, Q_rint(w * scale));
            R_DrawStretchPic(x, y, draw_w, height, COLOR_WHITE, pic);
            return draw_w;
        }
    }

    const char *key_label = ShortKeyLabel(keynum, label);
    return DrawKeyCap(x, y, height, color, key_label);
}

int UI_GetKeyIconWidth(int height, int keynum, const char *label)
{
    if (keynum >= 0) {
        qhandle_t pic;
        int w = 0;
        int h = 0;
        if (UI_GetBindIconForKey(keynum, &pic, &w, &h) && w > 0 && h > 0) {
            float scale = height > 0 ? (static_cast<float>(height) / static_cast<float>(h)) : 1.0f;
            return max(1, Q_rint(w * scale));
        }
    }

    const char *key_label = ShortKeyLabel(keynum, label);
    return KeyCapWidth(height, key_label);
}

int UI_DrawHintBar(const std::vector<UiHint> &left, const std::vector<UiHint> &right, int bottom)
{
    if (left.empty() && right.empty())
        return 0;

    int height = CONCHAR_HEIGHT;
    int icon_height = max(1, height - 2);
    int y = bottom - height + 1;

    color_t key_color = uis.color.normal;
    color_t text_color = COLOR_WHITE;

    int x = kHintPadding;
    for (size_t i = 0; i < left.size(); i++) {
        x += DrawHintItem(x, y, icon_height, left[i], key_color, text_color);
        if (i + 1 < left.size())
            x += kHintGap;
    }

    if (!right.empty()) {
        int total = 0;
        for (size_t i = 0; i < right.size(); i++) {
            const UiHint &hint = right[i];
            const char *key_label = hint.keyLabel.empty() ? nullptr : hint.keyLabel.c_str();
            int key_w = UI_GetKeyIconWidth(icon_height, hint.key, key_label);
            int item_w = key_w + kHintPadding + TextWidth(hint.label);
            total += item_w;
            if (i + 1 < right.size())
                total += kHintGap;
        }

        int rx = max(kHintPadding, uis.width - kHintPadding - total);
        for (size_t i = 0; i < right.size(); i++) {
            rx += DrawHintItem(rx, y, icon_height, right[i], key_color, text_color);
            if (i + 1 < right.size())
                rx += kHintGap;
        }
    }

    return height;
}

int UI_GetHintBarHeight(const std::vector<UiHint> &left, const std::vector<UiHint> &right)
{
    return (left.empty() && right.empty()) ? 0 : CONCHAR_HEIGHT;
}

} // namespace ui
