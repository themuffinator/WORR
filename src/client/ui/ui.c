/*
Copyright (C) 2003-2006 Andrey Nazarov

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

#include "ui.h"
#include "client/input.h"
#include "common/prompt.h"

uiStatic_t    uis;

LIST_DECL(ui_menus);

cvar_t    *ui_debug;
static cvar_t    *ui_open;
static cvar_t    *ui_scale;

// ===========================================================================

static void UI_GetVirtualScreen(int *width, int *height, float *pixel_scale)
{
    float scale_x = (float)r_config.width / VIRTUAL_SCREEN_WIDTH;
    float scale_y = (float)r_config.height / VIRTUAL_SCREEN_HEIGHT;
    float base_scale = max(scale_x, scale_y);
    int base_scale_int = (int)base_scale;

    if (base_scale_int < 1)
        base_scale_int = 1;

    int virtual_width = r_config.width / base_scale_int;
    int virtual_height = r_config.height / base_scale_int;

    if (virtual_width < 1)
        virtual_width = 1;
    if (virtual_height < 1)
        virtual_height = 1;

    float scale = (float)base_scale_int;

    if (width)
        *width = virtual_width;
    if (height)
        *height = virtual_height;
    if (pixel_scale)
        *pixel_scale = scale;
}

/*
=================
UI_PushMenu
=================
*/
void UI_PushMenu(menuFrameWork_t *menu)
{
    int i, j;

    if (!menu) {
        return;
    }

    // if this menu is already present, drop back to that level
    // to avoid stacking menus by hotkeys
    for (i = 0; i < uis.menuDepth; i++) {
        if (uis.layers[i] == menu) {
            break;
        }
    }

    if (i == uis.menuDepth) {
        if (uis.menuDepth >= MAX_MENU_DEPTH) {
            Com_EPrintf("UI_PushMenu: MAX_MENU_DEPTH exceeded\n");
            return;
        }
        uis.layers[uis.menuDepth++] = menu;
    } else {
        for (j = i; j < uis.menuDepth; j++) {
            UI_PopMenu();
        }
        uis.menuDepth = i + 1;
    }

    if (menu->push && !menu->push(menu)) {
        uis.menuDepth--;
        return;
    }

    Menu_Init(menu);

    Key_SetDest((Key_GetDest() & ~KEY_CONSOLE) | KEY_MENU);

    Con_Close(true);

    if (!uis.activeMenu) {
        // opening menu moves cursor to the nice location
        float pixel_scale = 1.0f;
        UI_GetVirtualScreen(NULL, NULL, &pixel_scale);
        IN_WarpMouse(Q_rint((menu->mins[0] / uis.scale) * pixel_scale),
                     Q_rint((menu->mins[1] / uis.scale) * pixel_scale));

        uis.mouseCoords[0] = menu->mins[0];
        uis.mouseCoords[1] = menu->mins[1];

        uis.entersound = true;
    }

    uis.activeMenu = menu;

    UI_DoHitTest();

    if (menu->expose) {
        menu->expose(menu);
    }
}

static void UI_Resize(void)
{
    int i;

    int base_width = r_config.width;
    int base_height = r_config.height;
    UI_GetVirtualScreen(&base_width, &base_height, NULL);

    uis.scale = R_ClampScale(ui_scale);
    uis.width = Q_rint(base_width * uis.scale);
    uis.height = Q_rint(base_height * uis.scale);
    uis.canvas_width = r_config.width;
    uis.canvas_height = r_config.height;

    for (i = 0; i < uis.menuDepth; i++) {
        Menu_Init(uis.layers[i]);
    }

    //CL_WarpMouse(0, 0);
}


/*
=================
UI_ForceMenuOff
=================
*/
void UI_ForceMenuOff(void)
{
    menuFrameWork_t *menu;
    int i;

    for (i = 0; i < uis.menuDepth; i++) {
        menu = uis.layers[i];
        if (menu->pop) {
            menu->pop(menu);
        }
    }

    Key_SetDest(Key_GetDest() & ~KEY_MENU);
    uis.menuDepth = 0;
    uis.activeMenu = NULL;
    uis.mouseTracker = NULL;
    uis.transparent = false;
}

/*
=================
UI_PopMenu
=================
*/
void UI_PopMenu(void)
{
    menuFrameWork_t *menu;

    Q_assert(uis.menuDepth > 0);

    menu = uis.layers[--uis.menuDepth];
    if (menu->pop) {
        menu->pop(menu);
    }

    if (!uis.menuDepth) {
        UI_ForceMenuOff();
        return;
    }

    uis.activeMenu = uis.layers[uis.menuDepth - 1];
    uis.mouseTracker = NULL;

    UI_DoHitTest();
}

/*
=================
UI_IsTransparent
=================
*/
bool UI_IsTransparent(void)
{
    if (!(Key_GetDest() & KEY_MENU)) {
        return true;
    }

    if (!uis.activeMenu) {
        return true;
    }

    return uis.activeMenu->transparent;
}

menuFrameWork_t *UI_FindMenu(const char *name)
{
    menuFrameWork_t *menu;

    LIST_FOR_EACH(menuFrameWork_t, menu, &ui_menus, entry) {
        if (!strcmp(menu->name, name)) {
            return menu;
        }
    }

    return NULL;
}

/*
=================
UI_OpenMenu
=================
*/
void UI_OpenMenu(uiMenu_t type)
{
    menuFrameWork_t *menu = NULL;

    if (!uis.initialized) {
        return;
    }

    // close any existing menus
    UI_ForceMenuOff();

    switch (type) {
    case UIMENU_DEFAULT:
        if (ui_open->integer) {
            menu = UI_FindMenu("main");
        }
        break;
    case UIMENU_MAIN:
        menu = UI_FindMenu("main");
        break;
    case UIMENU_GAME:
        menu = UI_FindMenu("game");
        if (!menu) {
            menu = UI_FindMenu("main");
        }
        break;
    case UIMENU_NONE:
        break;
    default:
        Q_assert(!"bad menu");
    }

    UI_PushMenu(menu);
}

//=============================================================================

/*
=================
UI_FormatColumns
=================
*/
void *UI_FormatColumns(int extrasize, ...)
{
    va_list argptr;
    char *buffer, *p;
    int i, j;
    size_t total = 0;
    char *strings[MAX_COLUMNS];
    size_t lengths[MAX_COLUMNS];

    va_start(argptr, extrasize);
    for (i = 0; i < MAX_COLUMNS; i++) {
        if ((p = va_arg(argptr, char *)) == NULL) {
            break;
        }
        strings[i] = p;
        total += lengths[i] = strlen(p) + 1;
    }
    va_end(argptr);

    buffer = UI_Malloc(extrasize + total + 1);
    p = buffer + extrasize;
    for (j = 0; j < i; j++) {
        memcpy(p, strings[j], lengths[j]);
        p += lengths[j];
    }
    *p = 0;

    return buffer;
}

char *UI_GetColumn(char *s, int n)
{
    int i;

    for (i = 0; i < n && *s; i++) {
        s += strlen(s) + 1;
    }

    return s;
}

/*
=================
UI_CursorInRect
=================
*/
bool UI_CursorInRect(const vrect_t *rect)
{
    if (uis.mouseCoords[0] < rect->x) {
        return false;
    }
    if (uis.mouseCoords[0] >= rect->x + rect->width) {
        return false;
    }
    if (uis.mouseCoords[1] < rect->y) {
        return false;
    }
    if (uis.mouseCoords[1] >= rect->y + rect->height) {
        return false;
    }
    return true;
}

// nb: all UI strings are drawn at full alpha
void UI_DrawString(int x, int y, int flags, color_t color, const char *string)
{
    if ((flags & UI_CENTER) == UI_CENTER) {
        x -= strlen(string) * CONCHAR_WIDTH / 2;
    } else if (flags & UI_RIGHT) {
        x -= strlen(string) * CONCHAR_WIDTH;
    }

    R_DrawString(x, y, flags, MAX_STRING_CHARS, string, COLOR_SETA_U8(color, 255), uis.fontHandle);
}

// nb: all UI chars are drawn at full alpha
void UI_DrawChar(int x, int y, int flags, color_t color, int ch)
{
    R_DrawChar(x, y, flags, ch, COLOR_SETA_U8(color, 255), uis.fontHandle);
}

void UI_StringDimensions(vrect_t *rc, int flags, const char *string)
{
    rc->height = CONCHAR_HEIGHT;
    rc->width = CONCHAR_WIDTH * strlen(string);

    if ((flags & UI_CENTER) == UI_CENTER) {
        rc->x -= rc->width / 2;
    } else if (flags & UI_RIGHT) {
        rc->x -= rc->width;
    }
}

void UI_DrawRect8(const vrect_t *rc, int border, int c)
{
    R_DrawFill8(rc->x, rc->y, border, rc->height, c);   // left
    R_DrawFill8(rc->x + rc->width - border, rc->y, border, rc->height, c);   // right
    R_DrawFill8(rc->x + border, rc->y, rc->width - border * 2, border, c);   // top
    R_DrawFill8(rc->x + border, rc->y + rc->height - border, rc->width - border * 2, border, c);   // bottom
}

//=============================================================================
/* Menu Subsystem */

/*
=================
UI_DoHitTest
=================
*/
bool UI_DoHitTest(void)
{
    menuCommon_t *item;

    if (!uis.activeMenu) {
        return false;
    }

    if (uis.mouseTracker) {
        item = uis.mouseTracker;
    } else {
        if (!(item = Menu_HitTest(uis.activeMenu))) {
            return false;
        }
    }

    if (!UI_IsItemSelectable(item)) {
        return false;
    }

    Menu_MouseMove(item);

    if (item->flags & QMF_HASFOCUS) {
        return false;
    }

    Menu_SetFocus(item);

    return true;
}

/*
=================
UI_MouseEvent
=================
*/
void UI_MouseEvent(int x, int y)
{
    x = Q_clip(x, 0, r_config.width - 1);
    y = Q_clip(y, 0, r_config.height - 1);

    float pixel_scale = 1.0f;
    UI_GetVirtualScreen(NULL, NULL, &pixel_scale);
    float inv_scale = (pixel_scale > 0.0f) ? (1.0f / pixel_scale) : 1.0f;
    uis.mouseCoords[0] = Q_rint((x * inv_scale) * uis.scale);
    uis.mouseCoords[1] = Q_rint((y * inv_scale) * uis.scale);

    UI_DoHitTest();
}

/*
=================
UI_Draw
=================
*/
void UI_Draw(unsigned realtime)
{
    int i;

    uis.realtime = realtime;

    if (!(Key_GetDest() & KEY_MENU)) {
        return;
    }

    if (!uis.activeMenu) {
        return;
    }

    R_SetScale(uis.scale);

    if (1) {
        // draw top menu
        if (uis.activeMenu->draw) {
            uis.activeMenu->draw(uis.activeMenu);
        } else {
            Menu_Draw(uis.activeMenu);
        }
    } else {
        // draw all layers
        for (i = 0; i < uis.menuDepth; i++) {
            if (uis.layers[i]->draw) {
                uis.layers[i]->draw(uis.layers[i]);
            } else {
                Menu_Draw(uis.layers[i]);
            }
        }
    }

    // draw custom cursor in fullscreen mode
    if (r_config.flags & QVF_FULLSCREEN) {
        R_DrawPic(uis.mouseCoords[0] - uis.cursorWidth / 2,
                  uis.mouseCoords[1] - uis.cursorHeight / 2, 
                  COLOR_WHITE, uis.cursorHandle);
    }

    if (ui_debug->integer) {
        UI_DrawString(uis.width - 4, 4, UI_RIGHT,
                      COLOR_WHITE, va("%3i %3i", uis.mouseCoords[0], uis.mouseCoords[1]));
    }

    // delay playing the enter sound until after the
    // menu has been drawn, to avoid delay while
    // caching images
    if (uis.entersound) {
        uis.entersound = false;
        S_StartLocalSound("misc/menu1.wav");
    }

    R_SetScale(1.0f);
}

void UI_StartSound(menuSound_t sound)
{
    switch (sound) {
    case QMS_IN:
        S_StartLocalSound("misc/menu1.wav");
        break;
    case QMS_MOVE:
        S_StartLocalSound("misc/menu2.wav");
        break;
    case QMS_OUT:
        S_StartLocalSound("misc/menu3.wav");
        break;
    case QMS_BEEP:
        S_StartLocalSound("misc/talk1.wav");
        break;
    default:
        break;
    }
}

/*
=================
UI_KeyEvent
=================
*/
void UI_KeyEvent(int key, bool down)
{
    menuSound_t sound;

    if (!uis.activeMenu) {
        return;
    }

    if (!down) {
        if (key == K_MOUSE1) {
            uis.mouseTracker = NULL;
        }
        return;
    }

    sound = Menu_Keydown(uis.activeMenu, key);

    UI_StartSound(sound);
}

/*
=================
UI_CharEvent
=================
*/
void UI_CharEvent(int key)
{
    menuCommon_t *item;
    menuSound_t sound;

    if (!uis.activeMenu) {
        return;
    }

    if ((item = Menu_ItemAtCursor(uis.activeMenu)) == NULL ||
        (sound = Menu_CharEvent(item, key)) == QMS_NOTHANDLED) {
        return;
    }

    UI_StartSound(sound);
}

static void UI_Menu_g(genctx_t *ctx)
{
    menuFrameWork_t *menu;

    LIST_FOR_EACH(menuFrameWork_t, menu, &ui_menus, entry)
        Prompt_AddMatch(ctx, menu->name);
}

static void UI_PushMenu_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        UI_Menu_g(ctx);
    }
}

static void UI_PushMenu_f(void)
{
    menuFrameWork_t *menu;
    char *s;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <menu>\n", Cmd_Argv(0));
        return;
    }
    s = Cmd_Argv(1);
    menu = UI_FindMenu(s);
    if (menu) {
        UI_PushMenu(menu);
    } else {
        Com_Printf("No such menu: %s\n", s);
    }
}

static void UI_PopMenu_f(void)
{
    if (uis.activeMenu) {
        UI_PopMenu();
    }
}


static const cmdreg_t c_ui[] = {
    { "forcemenuoff", UI_ForceMenuOff },
    { "pushmenu", UI_PushMenu_f, UI_PushMenu_c },
    { "popmenu", UI_PopMenu_f },

    { NULL, NULL }
};

static void ui_scale_changed(cvar_t *self)
{
    UI_Resize();
}

void UI_ModeChanged(void)
{
    ui_scale = Cvar_Get("ui_scale", "0", 0);
    ui_scale->changed = ui_scale_changed;
    UI_Resize();
}

static void UI_FreeMenus(void)
{
    menuFrameWork_t *menu, *next;

    LIST_FOR_EACH_SAFE(menuFrameWork_t, menu, next, &ui_menus, entry) {
        if (menu->free) {
            menu->free(menu);
        }
    }
    List_Init(&ui_menus);
}


/*
=================
UI_Init
=================
*/
void UI_Init(void)
{
    Cmd_Register(c_ui);

    ui_debug = Cvar_Get("ui_debug", "0", 0);
    ui_open = Cvar_Get("ui_open", "0", 0);

    UI_ModeChanged();

    uis.fontHandle = R_RegisterFont("conchars");
    uis.cursorHandle = R_RegisterPic("ch1");
    R_GetPicSize(&uis.cursorWidth, &uis.cursorHeight, uis.cursorHandle);

    for (int i = 0; i < NUM_CURSOR_FRAMES; i++) {
        uis.bitmapCursors[i] = R_RegisterPic(va("m_cursor%d", i));
    }

    uis.color.background    = COLOR_RGBA(0,   0,   0,   255);
    uis.color.normal        = COLOR_RGBA(15,  128, 235, 100);
    uis.color.active        = COLOR_RGBA(15,  128, 235, 100);
    uis.color.selection     = COLOR_RGBA(15,  128, 235, 100);
    uis.color.disabled      = COLOR_RGBA(127, 127, 127, 255);

    Q_strlcpy(uis.weaponModel, "w_railgun.md2", sizeof(uis.weaponModel));

    // load mapdb
    UI_MapDB_Init();

    // load custom menus
    UI_LoadScript();

    // load built-in menus
    M_Menu_PlayerConfig();
    M_Menu_Servers();
    M_Menu_Demos();

    Com_DPrintf("Registered %d menus.\n", List_Count(&ui_menus));

    uis.initialized = true;
}

/*
=================
UI_Shutdown
=================
*/
void UI_Shutdown(void)
{
    if (!uis.initialized) {
        return;
    }
    UI_ForceMenuOff();

    ui_scale->changed = NULL;

    PlayerModel_Free();

    UI_FreeMenus();

    Cmd_Deregister(c_ui);

    memset(&uis, 0, sizeof(uis));

    UI_MapDB_Shutdown();

    Z_LeakTest(TAG_UI);
}
